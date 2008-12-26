
//OpenSCADA system file: tmodschedul.cpp
/***************************************************************************
 *   Copyright (C) 2003-2008 by Roman Savochenko                           *
 *   rom_as@fromru.com                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; version 2 of the License.               *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>

#include <string>

#include "tsys.h"
#include "tmodschedul.h"

//*************************************************
//* TModSchedul                                   *
//*************************************************
TModSchedul::TModSchedul( ) :
    TSubSYS("ModSched","Modules sheduler",false), prcSt(false), mPer(10)
{
    //- Create calc timer -
    struct sigevent sigev;
    sigev.sigev_notify = SIGEV_THREAD;
    sigev.sigev_value.sival_ptr = this;
    sigev.sigev_notify_function = SchedTask;
    sigev.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME,&sigev,&tmId);
}

TModSchedul::~TModSchedul(  )
{
    timer_delete(tmId);
}

void TModSchedul::preDisable(int flag)
{
    //- Detach all share libs -
    ResAlloc res(hdRes,true);
    for( unsigned i_sh = 0; i_sh < SchHD.size(); i_sh++ )
	if( SchHD[i_sh].hd )
	{
	    while( SchHD[i_sh].use.size() )
	    {
		owner().at(TSYS::strSepParse(SchHD[i_sh].use[0],0,'.')).at().
			modDel(TSYS::strSepParse(SchHD[i_sh].use[0],1,'.'));
		SchHD[i_sh].use.erase(SchHD[i_sh].use.begin());
	    }
	    dlclose(SchHD[i_sh].hd);
	}
    res.release();
}

string TModSchedul::optDescr( )
{
    char buf[STR_BUF_LEN];
    snprintf(buf,sizeof(buf),_(
	"=================== Subsystem \"Module sheduler\" options =================\n"
	"    --ModPath=<path>   Modules <path> (/var/os/modules/).\n"
	"------------ Parameters of section <%s> in config file -----------\n"
	"ModPath  <path>        Path to shared libraries(modules).\n"
	"ModAuto  <list>        List of automatic loaded, attached and started shared libraries (direct_dbf.so;virt.so).\n"
	"ChkPer   <sec>         Period of checking at new shared libraries(modules).\n\n"
	),nodePath().c_str());

    return(buf);
}

void TModSchedul::subStart(  )
{
#if OSC_DEBUG >= 1
    mess_debug(nodePath().c_str(),_("Start subsystem."));
#endif

    //- Start interval timer for periodic thread creating -
    struct itimerspec itval;
    itval.it_interval.tv_sec = itval.it_value.tv_sec = chkPer();
    itval.it_interval.tv_nsec = itval.it_value.tv_nsec = 0;
    timer_settime(tmId, 0, &itval, NULL);
}

void TModSchedul::subStop(  )
{
#if OSC_DEBUG >= 1
    mess_debug(nodePath().c_str(),_("Stop subsystem."));
#endif

    //- Stop interval timer for periodic thread creating -
    struct itimerspec itval;
    itval.it_interval.tv_sec = itval.it_interval.tv_nsec =
	itval.it_value.tv_sec = itval.it_value.tv_nsec = 0;
    timer_settime(tmId, 0, &itval, NULL);
    if( TSYS::eventWait( prcSt, false, nodePath()+"stop",20) )
	throw TError(nodePath().c_str(),_("Module scheduler thread is not stoped!"));

#if OSC_DEBUG >= 2
    mess_debug(nodePath().c_str(),_("Stop subsystem. OK"));
#endif
}

void TModSchedul::setChkPer( int per )
{
    mPer = per;
    struct itimerspec itval;
    itval.it_interval.tv_sec = itval.it_value.tv_sec = chkPer();
    itval.it_interval.tv_nsec = itval.it_value.tv_nsec = 0;
    timer_settime(tmId, 0, &itval, NULL);
    modif();
}

void TModSchedul::SchedTask(union sigval obj)
{
    TModSchedul  *shed = (TModSchedul *)obj.sival_ptr;
    if( shed->prcSt )  return;
    shed->prcSt = true;

    try
    {
	shed->libLoad(SYS->modDir(),true);
    } catch(TError err){ mess_err(err.cat.c_str(),"%s",err.mess.c_str()); }

    shed->prcSt = false;
}

void TModSchedul::loadLibS(  )
{
    libLoad(SYS->modDir(),false);
}

void TModSchedul::load_( )
{
    //- Load parameters from command line -
    int next_opt;
    const char *short_opt="h";
    struct option long_opt[] =
    {
	{"help"     ,0,NULL,'h'},
	{"ModPath"  ,1,NULL,'m'},
	{NULL       ,0,NULL,0  }
    };

    optind=opterr=0;
    do
    {
	next_opt=getopt_long(SYS->argc,( char *const * ) SYS->argv,short_opt,long_opt,NULL);
	switch(next_opt)
	{
	    case 'h': fprintf(stdout,optDescr().c_str()); break;
	    case 'm': SYS->setModDir(optarg); break;
	    case -1 : break;
	}
    } while(next_opt != -1);

    //- Load parameters from command line -
    setChkPer( atoi(TBDS::genDBGet(nodePath()+"ChkPer",TSYS::int2str(mPer)).c_str()) );
    SYS->setModDir( TBDS::genDBGet(nodePath()+"ModPath",SYS->modDir()) );

    string opt = TBDS::genDBGet(nodePath()+"ModAuto");
    string ovl;
    mAmList.clear();
    for( int el_off = 0; (ovl=TSYS::strSepParse(opt,0,';',&el_off)).size(); )
        mAmList.push_back(ovl);
}

void TModSchedul::save_( )
{
    TBDS::genDBSet(nodePath()+"ChkPer",TSYS::int2str(chkPer()));
    string m_auto;
    for(int i_a = 0; i_a < mAmList.size(); i_a++ )
	m_auto+=mAmList[i_a]+";";
    TBDS::genDBSet(nodePath()+"ModAuto",m_auto);
}

void TModSchedul::ScanDir( const string &Paths, vector<string> &files )
{
    string NameMod, Path;

    files.clear();

    int ido, id=-1;
    do
    {
	ido=id+1; id = Paths.find(",",ido);

	dirent *scan_dirent;
	Path=Paths.substr(ido,id-ido);
	if(Path.size() <= 0) continue;

	DIR *IdDir = opendir(Path.c_str());
	if(IdDir == NULL) continue;

	while((scan_dirent = readdir(IdDir)) != NULL)
	{
	    if( string("..") == scan_dirent->d_name || string(".") == scan_dirent->d_name ) continue;
	    NameMod=Path+"/"+scan_dirent->d_name;
	    if( CheckFile(NameMod) ) files.push_back(NameMod); 
	}
	closedir(IdDir);

    } while(id != (int)string::npos);
}

bool TModSchedul::CheckFile( const string &iname )
{
    struct stat file_stat;
    string NameMod;

    stat(iname.c_str(),&file_stat);

    if( (file_stat.st_mode&S_IFMT) != S_IFREG ) return false;
    if( access(iname.c_str(),F_OK|R_OK) != 0 )  return false;
    NameMod=iname;

    void *h_lib = dlopen(iname.c_str(),RTLD_LAZY|RTLD_GLOBAL);
    if(h_lib == NULL)
    {
	mess_warning(nodePath().c_str(),_("SO <%s> error: %s !"),iname.c_str(),dlerror());
	return(false);
    }
    else dlclose(h_lib);

    for(unsigned i_sh=0; i_sh < SchHD.size(); i_sh++)
	if(SchHD[i_sh].name == iname )
	    if(file_stat.st_mtime > SchHD[i_sh].m_tm) return true;
	    else return false;

    return true;
}

int TModSchedul::libReg( const string &name )
{
    struct stat file_stat;

    ResAlloc res(hdRes,true);
    stat(name.c_str(),&file_stat);
    unsigned i_sh;
    for( i_sh = 0; i_sh < SchHD.size(); i_sh++ )
	if( SchHD[i_sh].name == name ) break;
    if( i_sh == SchHD.size() )	SchHD.push_back( SHD(NULL,file_stat.st_mtime,name) );
    else SchHD[i_sh].m_tm = file_stat.st_mtime;

    return i_sh;
}

void TModSchedul::libUnreg( const string &iname )
{
    ResAlloc res(hdRes,true);
    for(unsigned i_sh = 0; i_sh < SchHD.size(); i_sh++)
	if( SchHD[i_sh].name == iname ) 
	{
	    if( SchHD[i_sh].hd ) libDet( iname );
	    SchHD.erase(SchHD.begin()+i_sh);
	    return;
	}
    throw TError(nodePath().c_str(),_("SO <%s> is not present!"),iname.c_str());
}

void TModSchedul::libAtt( const string &iname, bool full )
{
    ResAlloc res(hdRes,true);
    for(unsigned i_sh = 0; i_sh < SchHD.size(); i_sh++)
	if( SchHD[i_sh].name == iname ) 
	{
	    if( SchHD[i_sh].hd ) 
		throw TError(nodePath().c_str(),_("SO <%s> is already attached!"),iname.c_str());

	    void *h_lib = dlopen(iname.c_str(),RTLD_LAZY|RTLD_GLOBAL);
	    if( !h_lib )
		throw TError(nodePath().c_str(),_("SO <%s> error: %s !"),iname.c_str(),dlerror());

	    //- Connect to module function -
	    TModule::SAt (*module)( int );
	    module = (TModule::SAt (*)(int)) dlsym(h_lib,"module");
	    if( dlerror() != NULL )
	    {
		dlclose(h_lib);
		throw TError(nodePath().c_str(),_("SO <%s> error: %s !"),iname.c_str(),dlerror());
	    }

	    //- Connect to attach function -
	    TModule *(*attach)( const TModule::SAt &, const string & );
	    attach = (TModule * (*)(const TModule::SAt &, const string &)) dlsym(h_lib,"attach");
	    if( dlerror() != NULL )
	    {
		dlclose(h_lib);
		throw TError(nodePath().c_str(),_("SO <%s> error: %s !"),iname.c_str(),dlerror());
	    }

	    struct stat file_stat;
	    stat(iname.c_str(),&file_stat);

	    int n_mod=0, add_mod=0;
	    TModule::SAt AtMod;
	    while( (AtMod = (module)( n_mod++ )).id.size() )
	    {
		vector<string> list;
		owner().list(list);
		for( unsigned i_sub = 0; i_sub < list.size(); i_sub++)
		{
		    if( owner().at(list[i_sub]).at().subModule() &&
			AtMod.type == owner().at(list[i_sub]).at().subId() )
		    {
			//-- Check type module version --
			if( AtMod.t_ver != owner().at(list[i_sub]).at().subVer() )
			{
			    mess_warning(nodePath().c_str(),_("%s for type <%s> doesn't support module version: %d!"),
				AtMod.id.c_str(),AtMod.type.c_str(),AtMod.t_ver);
			    break;
			}
			//-- Check module present --
			if( owner().at(list[i_sub]).at().modPresent(AtMod.id) )
			    mess_warning(nodePath().c_str(),_("Module <%s> is already present!"),AtMod.id.c_str());
			else
			{
			    //-- Attach new module --
			    TModule *LdMod = (attach)( AtMod, iname );
			    if( LdMod == NULL )
			    {
				mess_warning(nodePath().c_str(),_("Attach module <%s> error!"),AtMod.id.c_str());
				break;
			    }
			    //-- Add atached module --
			    owner().at(list[i_sub]).at().modAdd(LdMod);
			    SchHD[i_sh].use.push_back( list[i_sub]+"."+LdMod->modId() );
			    if(full)
			    {
				owner().at(list[i_sub]).at().load();
				owner().at(list[i_sub]).at().subStart();
			    }
			    add_mod++;
			    break;
			}
		    }
		}
	    }
	    if(add_mod == 0) dlclose(h_lib);
	    else SchHD[i_sh].hd = h_lib;
	    return;
	}
    throw TError(nodePath().c_str(),_("SO <%s> is not present!"),iname.c_str());
}

void TModSchedul::libDet( const string &iname )
{
    ResAlloc res(hdRes,true);
    for(unsigned i_sh = 0; i_sh < SchHD.size(); i_sh++)
    {
	if( SchHD[i_sh].name == iname && SchHD[i_sh].hd )
	{
	    while( SchHD[i_sh].use.size() )
	    {
		try
		{
		    owner().at(TSYS::strSepParse(SchHD[i_sh].use[0],0,'.')).at().
			    modAt(TSYS::strSepParse(SchHD[i_sh].use[0],1,'.')).at().modStop();
		    owner().at(TSYS::strSepParse(SchHD[i_sh].use[0],0,'.')).at().
			    modDel(TSYS::strSepParse(SchHD[i_sh].use[0],1,'.'));
		}catch(TError err)
		{
		    //owner().at(SchHD[i_sh]->use[0].mod_sub).at().modAt(SchHD[i_sh]->use[0].n_mod).at().load();
		    owner().at(TSYS::strSepParse(SchHD[i_sh].use[0],0,'.')).at().
			    modAt(TSYS::strSepParse(SchHD[i_sh].use[0],1,'.')).at().modStart();
		    throw;
		}
		SchHD[i_sh].use.erase(SchHD[i_sh].use.begin());
	    }
	    dlclose(SchHD[i_sh].hd);
	    SchHD[i_sh].hd = NULL;
	    return;
	}
    }
    throw TError(nodePath().c_str(),_("SO <%s> is not present!"),iname.c_str());
}

bool TModSchedul::CheckAuto( const string &name ) const
{
    if( mAmList.size() == 1 && mAmList[0] == "*") return(true);
    else
	for( unsigned i_au = 0; i_au < mAmList.size(); i_au++)
	    if( name == mAmList[i_au] ) return true;

    return false;
}

void TModSchedul::libList( vector<string> &list )
{
    ResAlloc res(hdRes,false);
    list.clear();
    for(unsigned i_sh = 0; i_sh < SchHD.size(); i_sh++)
	list.push_back( SchHD[i_sh].name );
}

TModSchedul::SHD &TModSchedul::lib( const string &iname )
{
    ResAlloc res(hdRes,false);
    //string nm_t = SYS->fNameFix(name);
    for(unsigned i_sh = 0; i_sh < SchHD.size(); i_sh++)
	if( SchHD[i_sh].name == iname ) 
	    return SchHD[i_sh];
    throw TError(nodePath().c_str(),_("SO <%s> is not present!"),iname.c_str());
}

void TModSchedul::libLoad( const string &iname, bool full)
{
    vector<string> files;

    ScanDir( iname, files );
    for(unsigned i_f = 0; i_f < files.size(); i_f++)
    {
	unsigned i_sh;
	bool st_auto = CheckAuto(files[i_f]);
	for( i_sh = 0; i_sh < SchHD.size(); i_sh++ )
	    if( SchHD[i_sh].name == files[i_f] ) break;
	if(i_sh < SchHD.size())
	{
	    try { if(st_auto) libDet(files[i_f]); }
	    catch(TError err)
	    {
		mess_warning(err.cat.c_str(),"%s",err.mess.c_str());
		mess_warning(nodePath().c_str(),_("Can't detach library <%s>."),files[i_f].c_str());
		continue;
	    }
	}
	libReg(files[i_f]);
	if(st_auto)
	{
	    try{ libAtt(files[i_f],full); }
	    catch( TError err ){ mess_warning(err.cat.c_str(),"%s",err.mess.c_str()); }
	}
    }
}

void TModSchedul::cntrCmdProc( XMLNode *opt )
{
    //- Get page info -
    if( opt->name() == "info" )
    {
	TSubSYS::cntrCmdProc(opt);
	if(ctrMkNode("area",opt,0,"/ms",_("Subsystem"),0444,"root","root"))
	{
	    ctrMkNode("fld",opt,-1,"/ms/chk_per",_("Check modules period (sec)"),0664,"root","root",1,"tp","dec");
	    ctrMkNode("comm",opt,-1,"/ms/chk_now",_("Check modules now."),0660,"root","root");
	    ctrMkNode("fld",opt,-1,"/ms/mod_path",_("Path to shared libs(modules)"),0444,"root","root",1,"tp","str");
	    ctrMkNode("list",opt,-1,"/ms/mod_auto",_("List of auto conected shared libs(modules)"),0664,"root","root",2,"tp","str","s_com","add,ins,edit,del");
	}
	ctrMkNode("fld",opt,-1,"/help/g_help",_("Options help"),0440,"root","root",3,"tp","str","cols","90","rows","10");
	return;
    }

    //- Process command to page -
    string a_path = opt->attr("path");
    if( a_path == "/ms/chk_per" )
    {
	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->setText(TSYS::int2str(chkPer()));
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )	setChkPer(atoi(opt->text().c_str()));
    }
    else if( a_path == "/ms/mod_path" && ctrChkNode(opt,"get") )	opt->setText( SYS->modDir() );
    else if( a_path == "/ms/mod_auto" )
    {
	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )
	    for( unsigned i_a=0; i_a < mAmList.size(); i_a++ )
		opt->childAdd("el")->setText(mAmList[i_a]);
	if( ctrChkNode(opt,"add",0664,"root","root",SEQ_WR) )	{ mAmList.push_back(opt->text()); modif(); }
	if( ctrChkNode(opt,"ins",0664,"root","root",SEQ_WR) )	{ mAmList.insert(mAmList.begin()+atoi(opt->attr("pos").c_str()),opt->text()); modif(); }
	if( ctrChkNode(opt,"edit",0664,"root","root",SEQ_WR) )	{ mAmList[atoi(opt->attr("pos").c_str())] = opt->text(); modif(); }
	if( ctrChkNode(opt,"del",0664,"root","root",SEQ_WR) )	{ mAmList.erase(mAmList.begin()+atoi(opt->attr("pos").c_str())); modif(); }
    }
    else if( a_path == "/help/g_help" && ctrChkNode(opt,"get",0440,"root","root",SEQ_RD) )	opt->setText(optDescr());
    else if( a_path == "/ms/chk_now" && ctrChkNode(opt,"set",0660,"root","root",SEQ_WR) )	libLoad(SYS->modDir(),true);
    else TSubSYS::cntrCmdProc(opt);
}
