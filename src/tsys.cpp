
//OpenSCADA system file: tsys.cpp
/***************************************************************************
 *   Copyright (C) 2003-2006 by Roman Savochenko                           *
 *   rom_as@fromru.com                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
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


#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <langinfo.h>

#include "../config.h"
#include "terror.h"
#include "tmess.h"
#include "tsys.h"

//Continuously access variable
TMess 	*Mess;
TSYS  	*SYS;

TSYS::TSYS( int argi, char ** argb, char **env ) : 
    m_confFile("/etc/oscada.xml"), m_id("EmptySt"), m_name("Empty Station"),
    m_user("root"),argc(argi), envp((const char **)env), argv((const char **)argb), stop_signal(0), 
    m_sysOptCfg(false), mWorkDB(""), mSaveAtExit(false)
{
    SYS = this;		//Init global access value
    m_subst = grpAdd("sub_",true);
    nodeEn();
    
    Mess = new TMess();

    if( getenv("USER") ) m_user = getenv("USER");
    
    //Init system clock
    clkCalc();
    
    signal(SIGINT,sighandler);
    signal(SIGTERM,sighandler);
    //signal(SIGCHLD,sighandler);
    signal(SIGALRM,sighandler);
    signal(SIGPIPE,sighandler);
    //signal(SIGSEGV,sighandler);
    signal(SIGABRT,sighandler);
}

TSYS::~TSYS(  )
{
    //Delete all nodes in order
    del("ModSched");
    del("UI");
    del("Special");
    del("Archive");
    del("DAQ");
    del("Protocol");
    del("Transport");
    del("Security");
    del("BD");
    
    delete Mess;
}

string TSYS::int2str( int val, TSYS::IntView view )
{
    char buf[STR_BUF_LEN];
    if( view == TSYS::Dec )      snprintf(buf,sizeof(buf),"%d",val); 
    else if( view == TSYS::Oct ) snprintf(buf,sizeof(buf),"%o",val); 
    else if( view == TSYS::Hex ) snprintf(buf,sizeof(buf),"%x",val);

    return buf;
}

string TSYS::ll2str( long long val, IntView view )
{
    char buf[STR_BUF_LEN];
    if( view == TSYS::Dec )      snprintf(buf,sizeof(buf),"%lld",val);
    else if( view == TSYS::Oct ) snprintf(buf,sizeof(buf),"%llo",val);
    else if( view == TSYS::Hex ) snprintf(buf,sizeof(buf),"%llx",val);
		
    return buf;
}

string TSYS::real2str( double val )
{
    char buf[STR_BUF_LEN];
    snprintf(buf,sizeof(buf),"%g",val); 

    return buf;
}

bool TSYS::strEmpty( const string &val )
{
    if( !val.size() )	return true;
    for( int i_s = 0; i_s < val.size(); i_s++ )
	if( val[i_s] != ' ' && val[i_s] != '\n' && val[i_s] != '\t' )
	    return false;
    
    return true;
}

string TSYS::optDescr( )
{
    char s_buf[STR_BUF_LEN];
    utsname buf;

    uname(&buf);
    snprintf(s_buf,sizeof(s_buf),_(
	"***************************************************************************\n"
	"********** %s v%s (%s-%s). *********\n"
	"***************************************************************************\n\n"
	"===========================================================================\n"
	"========================= The general system options ======================\n"
	"===========================================================================\n"
	"-h, --help             Info message about system options.\n"
	"    --Config=<path>    Config file path.\n"
	"    --Station=<name>   Station name.\n"
	"    --demon            Start into demon mode.\n"
	"    --MessLev=<level>  Process messages <level> (0-7).\n"
    	"    --log=<direct>     Direct messages to:\n"
    	"                         <direct> & 1 - syslogd;\n"
    	"                         <direct> & 2 - stdout;\n"
    	"                         <direct> & 4 - stderr;\n"
	"                         <direct> & 8 - archive.\n"
    	"----------- The config file station <%s> parameters -----------\n"
	"Workdir    <path>	Work directory.\n"
    	"MessLev    <level>     Messages <level> (0-7).\n"
    	"LogTarget  <direction> Direct messages to:\n"
    	"                           <direct> & 1 - syslogd;\n"
    	"                           <direct> & 2 - stdout;\n"
    	"                           <direct> & 4 - stderr;\n"
	"                           <direct> & 8 - archive.\n"
	"SysLang    <lang>	Internal language.\n"
    	"WorkDB     <Type.Name> Work DB (type and name).\n"
	"SaveAtExit <true>      Save system at exit.\n"
	"SYSOptCfg  <true>      Get system options from DB.\n\n"),
	PACKAGE_NAME,VERSION,buf.sysname,buf.release,nodePath().c_str());
		
    return s_buf;
}

bool TSYS::cfgFileLoad()
{	
    bool cmd_help = false;    

    //================ Load parameters from commandline =========================
    int next_opt;
    char *short_opt="h";
    struct option long_opt[] =
    {
	{"help"     ,0,NULL,'h'},
	{"Config"   ,1,NULL,'f'},
	{"Station"  ,1,NULL,'s'},
	{NULL       ,0,NULL,0  }
    };

    optind=opterr=0;
    do
    {
	next_opt=getopt_long(argc,(char * const *)argv,short_opt,long_opt,NULL);
	switch(next_opt)
	{
	    case 'h': 
		fprintf(stdout,optDescr().c_str()); 
		Mess->messLevel(7);
		cmd_help = true; 
		break;
	    case 'f': m_confFile = optarg; break;
	    case 's': m_id = optarg; break;
	    case -1 : break;
	}
    } while(next_opt != -1);

    //Load config file
    int hd = open(m_confFile.c_str(),O_RDONLY);
    if( hd < 0 ) 
	mess_err(nodePath().c_str(),_("Config file <%s> error: %s"),m_confFile.c_str(),strerror(errno));
    else
    {
	int cf_sz = lseek(hd,0,SEEK_END);
	lseek(hd,0,SEEK_SET);
	char *buf = (char *)malloc(cf_sz+1);
	read(hd,buf,cf_sz);
	buf[cf_sz] = 0;
	close(hd);
	string s_buf = buf;
	free(buf);
	try
	{ 
	    root_n.load(s_buf); 
	    if( root_n.name() == "OpenSCADA" )
	    {
		XMLNode *stat_n = NULL;
		for( int i_st = root_n.childSize()-1; i_st >= 0; i_st-- )
		    if( root_n.childGet(i_st)->name() == "station" )
		    {
			stat_n = root_n.childGet(i_st);
    			if( stat_n->attr("id") == m_id ) break;
		    }
                if( stat_n && stat_n->attr("id") != m_id )
                {
		    mess_warning(nodePath().c_str(),_("Station <%s> into config file no present. Use <%s> station config!"),
                        m_id.c_str(), stat_n->attr("id").c_str() );
  		    m_id 	= stat_n->attr("id");
		    m_name 	= stat_n->attr("name");
		}
		if( !stat_n )	root_n.clear();
	    } else root_n.clear();
	    if( !root_n.childSize() )
		mess_err(nodePath().c_str(),_("Config <%s> error!"),m_confFile.c_str());
	}
	catch( TError err ) { mess_err(nodePath().c_str(),_("Load config file error: %s"),err.mess.c_str() ); }
    }
    
    return cmd_help;
}

void TSYS::cfgPrmLoad()
{
    //System parameters
    m_sysOptCfg = atoi(TBDS::genDBGet(nodePath()+"SYSOptCfg",TSYS::int2str(m_sysOptCfg),"root",true).c_str());
    chdir(TBDS::genDBGet(nodePath()+"Workdir","","root",sysOptCfg()).c_str());
    
    mWorkDB = TBDS::genDBGet(nodePath()+"WorkDB","*.*","root",sysOptCfg());
    mSaveAtExit = atoi(TBDS::genDBGet(nodePath()+"SaveAtExit","0","root",sysOptCfg()).c_str());
}

void TSYS::load()
{
    static bool first_load = true;    
    
    if(first_load)
    {
	add(new TBDS());
	add(new TSecurity());
	add(new TTransportS());
	add(new TProtocolS());
	add(new TDAQS());
	add(new TArchiveS());
	add(new TSpecialS());
	add(new TUIS());
	add(new TModSchedul());
    }
    
    bool cmd_help = cfgFileLoad();
    mess_info(nodePath().c_str(),_("Load!"));
    cfgPrmLoad();
    Mess->load();	//Messages load
    
    if(first_load)
    {
    	//Load modules
    	modSchedul().at().subLoad();
    	modSchedul().at().loadLibS();
	//Second load for load from generic DB	
	cfgPrmLoad();
	Mess->load();
    }

    //================== Load subsystems and modules ============
    vector<string> lst;
    list(lst);
    for( unsigned i_a=0; i_a < lst.size(); i_a++ )
        try{ at(lst[i_a]).at().subLoad(); }
        catch(TError err) 
        { 
    	    mess_err(err.cat.c_str(),"%s",err.mess.c_str());
	    mess_err(nodePath().c_str(),_("Error load subsystem <%s>."),lst[i_a].c_str());
	}
    
    mess_debug(nodePath().c_str(),_("Load OK!"));
    
    if( cmd_help ) throw TError(nodePath().c_str(),"Command line help call.");
    first_load = false;
}

void TSYS::save( )
{
    char buf[STR_BUF_LEN];
    
    mess_info(nodePath().c_str(),_("Save!"));
    
    //System parameters
    getcwd(buf,sizeof(buf));
    TBDS::genDBSet(nodePath()+"Workdir",buf);
    TBDS::genDBSet(nodePath()+"WorkDB",mWorkDB);
    TBDS::genDBSet(nodePath()+"SaveAtExit",TSYS::int2str(mSaveAtExit));
    
    Mess->save();       //Messages load
    
    vector<string> lst;
    list(lst);
    for( unsigned i_a=0; i_a < lst.size(); i_a++ )
        try{ at(lst[i_a]).at().subSave(); }
        catch(TError err) 
	{ 
	    mess_err(err.cat.c_str(),"%s",err.mess.c_str());
	    mess_err(nodePath().c_str(),_("Error save subsystem <%s>."),lst[i_a].c_str());
	}
    
    mess_debug(nodePath().c_str(),_("Save OK!"));
}

int TSYS::start(  )
{
    vector<string> lst;
    list(lst);
    
    mess_info(nodePath().c_str(),_("Start!"));
    for( unsigned i_a=0; i_a < lst.size(); i_a++ )
	try{ at(lst[i_a]).at().subStart(); }
	catch(TError err) 
	{ 
	    mess_err(err.cat.c_str(),"%s",err.mess.c_str()); 
	    mess_err(nodePath().c_str(),_("Error start subsystem <%s>."),lst[i_a].c_str()); 
	}	    
    mess_debug(nodePath().c_str(),_("Start OK!"));
    
    cfgFileScan( true );
    int i_cnt = 0;    
    while(!stop_signal)	
    {
	if( ++i_cnt > 10*1000/STD_WAIT_DELAY )  //10 second
	{
	    i_cnt = 0;
	    clkCalc( );
    	    cfgFileScan( );	
	}
       	usleep( STD_WAIT_DELAY*1000 ); 
    }
    
    mess_info(nodePath().c_str(),_("Stop!"));  
    if(saveAtExit())	save();
    for( int i_a=lst.size()-1; i_a >= 0; i_a-- )
	try { at(lst[i_a]).at().subStop(); }
	catch(TError err) 
	{ 
	    mess_err(err.cat.c_str(),"%s",err.mess.c_str());
	    mess_err(nodePath().c_str(),_("Error stop subsystem <%s>."),lst[i_a].c_str());
	}
    mess_debug(nodePath().c_str(),_("Stop OK!"));

    return stop_signal;       
}

void TSYS::stop( )    
{ 
    stop_signal = SIGINT;
}

void TSYS::sighandler( int signal )
{
    switch(signal)
    {
	case SIGINT: 
	    SYS->stop_signal=signal; 
	    break;
	case SIGTERM:
	    mess_warning(SYS->nodePath().c_str(),_("Have get a Terminate signal. Server been stoped!"));
	    SYS->stop_signal=signal;
	    break;
	case SIGCHLD:
	{
	    int status;
	    pid_t pid = wait(&status);
	    if(!WIFEXITED(status) && pid > 0)
		mess_info(SYS->nodePath().c_str(),_("Free child process %d!"),pid);
	    break;
	}
	case SIGPIPE:	
	    //mess_warning(SYS->nodePath().c_str(),_("Broken PIPE signal!"));
	    break;
	case SIGSEGV:
	    mess_emerg(SYS->nodePath().c_str(),_("Segmentation fault signal!"));
	    break;
	case SIGABRT:
	    mess_emerg(SYS->nodePath().c_str(),_("OpenSCADA aborted!"));
	    break;
	case SIGALRM:	break;    
	default:
	    mess_warning(SYS->nodePath().c_str(),_("Unknown signal %d!"),signal);
    }
}

void TSYS::cfgFileScan( bool first )
{
    static string cfg_fl;
    static struct stat f_stat;
    
    struct stat f_stat_t;
    bool   up = false;

    if(cfg_fl == cfgFile())
    {
	stat(cfg_fl.c_str(),&f_stat_t);
	if( f_stat.st_mtime != f_stat_t.st_mtime ) up = true;
    }
    else up = true;
    cfg_fl = cfgFile();
    stat(cfg_fl.c_str(),&f_stat);
    if(up == true && !first )
    {
	load();
	Mess->load();
	
	vector<string> lst;
	list( lst );
	for( unsigned i_sub = 0; i_sub < lst.size(); i_sub++)
    	    at(lst[i_sub]).at().subLoad();
    }    
}

long long TSYS::curTime()
{
    timeval cur_tm;
    gettimeofday(&cur_tm,NULL);
    return (long long)cur_tm.tv_sec*1000000 + cur_tm.tv_usec;
}

string TSYS::fNameFix( const string &fname )
{
    string tmp;
    char   buf[1024];   //!!!!
    
    for( string::const_iterator it = fname.begin(); it != fname.end(); it++ )
    {	
	if( *(it) == '.' && *(it+1) == '.' && *(it+2) == '/')
	{
	    string cur = getcwd(buf,sizeof(buf));
	    if(chdir("..") != 0) return "";
	    tmp += getcwd(buf,sizeof(buf));
	    chdir(cur.c_str());       	
	    it++;
	}
	else if( *(it) == '.' && *(it+1) == '/' ) 
	    tmp += getcwd(buf,sizeof(buf));
	else tmp += *(it);
    }
    return tmp;
}

bool TSYS::eventWait( bool &m_mess_r_stat, bool exempl, const string &loc, time_t tm )
{
    time_t t_tm, s_tm;
    
    t_tm = s_tm = time(NULL);
    while( m_mess_r_stat != exempl )
    {
	time_t c_tm = time(NULL);
	//Check timeout
	if( tm && ( c_tm > s_tm+tm) )
	{
	    mess_crit(loc.c_str(),_("Timeouted !!!"));
    	    return true;
	}
	//Make messages
	if( c_tm > t_tm+1 )  //1sec 
	{
	    t_tm = c_tm;
	    mess_info(loc.c_str(),_("Wait event..."));
	}
	usleep(STD_WAIT_DELAY*1000);
    }
    return false;
}

string TSYS::strSepParse( const string &path, int level, char sep )
{
    int an_dir = 0, t_lev = 0;
    while(true)
    {
        int t_dir = path.find(sep,an_dir);
		    
        if( t_lev++ == level )	return path.substr(an_dir,t_dir-an_dir);
        if( t_dir == string::npos ) return "";
        an_dir = t_dir+1;
    }
}		

string TSYS::pathLev( const string &path, int level, bool encode )
{
    int an_dir = 0, t_lev = 0;
    while(path[an_dir]=='/') an_dir++;
    while(true)
    {
        int t_dir = path.find("/",an_dir);
			
        if( t_lev++ == level )
        {
            if( encode ) return strDecode(path.substr(an_dir,t_dir-an_dir),TSYS::PathEl);
            return path.substr(an_dir,t_dir-an_dir);
        }
	if( t_dir == string::npos ) return "";
        an_dir = t_dir;
        while( an_dir < path.size() && path[an_dir]=='/') an_dir++;
    }
}

string TSYS::strEncode( const string &in, TSYS::Code tp, const string &symb )
{
    int i_sz;
    string sout = in;
    
    switch(tp)	
    {
	case TSYS::PathEl:
	    for( i_sz = 0; i_sz < sout.size(); i_sz++ )
		switch(sout[i_sz])
		{
		    case '/': sout.replace(i_sz,1,"%2f"); i_sz+=2; break;
		    case '%': sout.replace(i_sz,1,"%25"); i_sz+=2; break;
		}
	    break;	
	case TSYS::HttpURL:
	    for( i_sz = 0; i_sz < sout.size(); i_sz++ )
		switch(sout[i_sz])
		{
		    case '%': sout.replace(i_sz,1,"%25"); i_sz+=2; break;
		    case ' ': sout.replace(i_sz,1,"%20"); i_sz+=2; break;
		    case '\t': sout.replace(i_sz,1,"%09"); i_sz+=2; break;
		    default:
			if( sout[i_sz]&0x80 )
			{
			    char buf[4];
		    	    snprintf(buf,sizeof(buf),"%%%02X",(unsigned char)sout[i_sz]);
			    sout.replace(i_sz,1,buf);
			    i_sz+=2;
			    break;
			}
		}	
	    break;	
	case TSYS::Html:
	    for( i_sz = 0; i_sz < sout.size(); i_sz++ )
		switch(sout[i_sz])
		{ 
		    case '>': sout.replace(i_sz,1,"&gt;"); i_sz+=3; break;
		    case '<': sout.replace(i_sz,1,"&lt;"); i_sz+=3; break;
		    case '"': sout.replace(i_sz,1,"&quot;"); i_sz+=5; break;
		    case '&': sout.replace(i_sz,1,"&amp;"); i_sz+=4; break;
		    case '\'': sout.replace(i_sz,1,"&#039;"); i_sz+=5; break;			     
		}
	    break;	
	case TSYS::JavaSc:
	    for( i_sz = 0; i_sz < sout.size(); i_sz++ )
                if( sout[i_sz] == '\n' ) { sout.replace(i_sz,1,"\\n"); i_sz++; }	
	    break;
	case TSYS::SQL:
	    for( i_sz = 0; i_sz < sout.size(); i_sz++ )
		switch(sout[i_sz])
		{
		    case '\'': sout.replace(i_sz,1,"\\'"); i_sz++; break;
		    case '\"': sout.replace(i_sz,1,"\\\""); i_sz++; break;
		    case '`':  sout.replace(i_sz,1,"\\`"); i_sz++; break;
		    case '\\': sout.replace(i_sz,1,"\\\\"); i_sz++; break;
		}	    	
            break;
	case TSYS::Custom:
	    for( i_sz = 0; i_sz < sout.size(); i_sz++ )
		for( int i_smb = 0; i_smb < symb.size(); i_smb++ )
		    if( sout[i_sz] == symb[i_smb] )
		    {
			char buf[4];
                        sprintf(buf,"%%%02X",(unsigned char)sout[i_sz]);
                        sout.replace(i_sz,1,buf);
                        i_sz+=2;
			break;
		    }
	    break;
	case TSYS::base64:
	{
	    sout = "";
	    char *base64alph = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	    for( i_sz = 0; i_sz < in.size(); i_sz+=3 )
	    {
		if(i_sz && !(i_sz%57))	sout.push_back('\n');
		sout.push_back(base64alph[(unsigned char)in[i_sz]>>2]);
		if((i_sz+1) >= in.size()) 
		{
		    sout.push_back(base64alph[((unsigned char)in[i_sz]&0x03)<<4]);
		    sout+="==";
		}
		else
		{
		    sout.push_back(base64alph[(((unsigned char)in[i_sz]&0x03)<<4)|((unsigned char)in[i_sz+1]>>4)]);
		    if((i_sz+2) >= in.size()) 
		    {
			sout.push_back(base64alph[((unsigned char)in[i_sz+1]&0x0F)<<2]);
			sout.push_back('=');
		    }
		    else 
		    {
			sout.push_back(base64alph[(((unsigned char)in[i_sz+1]&0x0F)<<2)|((unsigned char)in[i_sz+2]>>6)]);
			sout.push_back(base64alph[(unsigned char)in[i_sz+2]&0x3F]);
		    }
		}
	    }
	    break;
	}
	case TSYS::FormatPrint:
	    for( i_sz = 0; i_sz < sout.size(); i_sz++ )
                if( sout[i_sz] == '%' ) { sout.replace(i_sz,1,"%%"); i_sz++; }
	    break;
    }
    return sout;
}

unsigned char TSYS::getBase64Code(unsigned char asymb) 
{
    switch(asymb)
    {
	case 'A' ... 'Z':	return asymb-(unsigned char)'A';
	case 'a' ... 'z':	return 26+asymb-(unsigned char)'a';
	case '0' ... '9':	return 52+asymb-(unsigned char)'0';
	case '+': return 62;		    
	case '/': return 63;		
    }
}

string TSYS::strDecode( const string &in, TSYS::Code tp )
{
    int i_sz;
    string sout = in;

    switch(tp)
    {
	case TSYS::PathEl: case TSYS::HttpURL: case TSYS::Custom:
	    for( i_sz = 0; i_sz < (int)sout.size()-2; i_sz++ )
		if( sout[i_sz] == '%' )
		    sout.replace(i_sz,3,1,(char)strtol(sout.substr(i_sz+1,2).c_str(),NULL,16));
	    break;
	case TSYS::base64:
	    sout = "";
	    for( i_sz = 0; i_sz < in.size(); )
	    {
		if(in[i_sz] == '\n')	i_sz+=sizeof('\n');
		if((i_sz+3) < in.size())
		    if( in[i_sz+1] != '=' )
		    {
			char w_code1 = TSYS::getBase64Code(in[i_sz+1]);
			sout.push_back((TSYS::getBase64Code(in[i_sz])<<2)|(w_code1>>4));
			if( in[i_sz+2] != '=' )
			{
			    char w_code2 = TSYS::getBase64Code(in[i_sz+2]);
			    sout.push_back((w_code1<<4)|(w_code2>>2));
			    if( in[i_sz+3] != '=' )
				sout.push_back((w_code2<<6)|TSYS::getBase64Code(in[i_sz+3]));
			}
		    }
		i_sz+=4;
	    }
	    break;
    }
	    
    return sout;
}

long TSYS::HZ()
{
    return sysconf(_SC_CLK_TCK);
}

void TSYS::cntrCmdProc( XMLNode *opt )
{
    char buf[STR_BUF_LEN];
    
    //Get page info
    if( opt->name() == "info" )
    {
	snprintf(buf,sizeof(buf),_("%s station: \"%s\""),PACKAGE_NAME,name().c_str());
	ctrMkNode("oscada_cntr",opt,-1,"/",buf,0444);
	if(ctrMkNode("branches",opt,-1,"/br","",0444))
	    ctrMkNode("grp",opt,-1,"/br/sub_",_("Subsystem"),0444,"root","root",1,"list","/subs/br");	
	if(TUIS::icoPresent(id())) ctrMkNode("img",opt,-1,"/ico","",0444);
	if(ctrMkNode("area",opt,-1,"/gen",_("Station"),0444))
	{
	    ctrMkNode("fld",opt,-1,"/gen/stat",_("Station"),0444,"root","root",1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/prog",_("Programm"),0444,"root","root",1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/ver",_("Version"),0444,"root","root",1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/host",_("Host name"),0444,"root","root",1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/user",_("System user"),0444,"root","root",1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/sys",_("Operation system"),0444,"root","root",1,"tp","str");
    	    ctrMkNode("fld",opt,-1,"/gen/frq",_("Frequency (MHZ)"),0444,"root","root",1,"tp","real");
	    ctrMkNode("fld",opt,-1,"/gen/clk_res",_("Realtime clock resolution (msec)"),0444,"root","root",1,"tp","real");
	    ctrMkNode("fld",opt,-1,"/gen/in_charset",_("Internal charset"),0440,"root","root",1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/config",_("Config file"),0440,"root","root",1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/workdir",_("Work directory"),0664,"root","root",1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/wrk_db",_("Work DB (module.bd)"),0660,"root",db().at().subId().c_str(),1,"tp","str");
	    ctrMkNode("fld",opt,-1,"/gen/saveExit",_("Save system at exit"),0664,"root","root",1,"tp","bool");
	    ctrMkNode("fld",opt,-1,"/gen/lang",_("Language"),0664,"root","root",1,"tp","str");
	    if(ctrMkNode("area",opt,-1,"/gen/mess",_("Messages"),0444))
	    {
		ctrMkNode("fld",opt,-1,"/gen/mess/lev",_("Least level"),0664,"root","root",2,"tp","dec","len","1");
 		ctrMkNode("fld",opt,-1,"/gen/mess/log_sysl",_("To syslog"),0664,"root","root",1,"tp","bool");
		ctrMkNode("fld",opt,-1,"/gen/mess/log_stdo",_("To stdout"),0664,"root","root",1,"tp","bool");
		ctrMkNode("fld",opt,-1,"/gen/mess/log_stde",_("To stderr"),0664,"root","root",1,"tp","bool");
		ctrMkNode("fld",opt,-1,"/gen/mess/log_arch",_("To archive"),0664,"root","root",1,"tp","bool");
	    }
	    ctrMkNode("comm",opt,-1,"/gen/load",_("Load system"),0660);
	    ctrMkNode("comm",opt,-1,"/gen/save",_("Save system"),0660);
	}
	if(ctrMkNode("area",opt,-1,"/subs",_("Subsystems")))
	    ctrMkNode("list",opt,-1,"/subs/br",_("Subsystems"),0444,"root","root",2,"tp","br","br_pref","sub_");
	if(ctrMkNode("area",opt,-1,"/hlp",_("Help")))
	    ctrMkNode("fld",opt,-1,"/hlp/g_help",_("Options help"),0440,"root","root",3,"tp","str","cols","90","rows","10");
	return;
    }    
    
    //Process command to page
    string a_path = opt->attr("path");
    if( a_path == "/ico" && ctrChkNode(opt) )
    {
	string itp;
        opt->text(TSYS::strEncode(TUIS::icoGet(id(),&itp),TSYS::base64));
        opt->attr("tp",itp);	
    }	
    else if(  a_path == "/gen/host" && ctrChkNode(opt) )	
    {
	utsname ubuf; uname(&ubuf);
	opt->text(ubuf.nodename);
    }
    else if( a_path == "/gen/sys" && ctrChkNode(opt) )  	
    {
	utsname ubuf; uname(&ubuf);
	opt->text(string(ubuf.sysname)+"-"+ubuf.release);	
    }
    else if( a_path == "/gen/user" && ctrChkNode(opt) )	opt->text(m_user);
    else if( a_path == "/gen/prog" && ctrChkNode(opt) )	opt->text(PACKAGE_NAME);
    else if( a_path == "/gen/ver" && ctrChkNode(opt) )	opt->text(VERSION);
    else if( a_path == "/gen/stat" && ctrChkNode(opt) )	opt->text(name());
    else if( a_path == "/gen/frq" && ctrChkNode(opt) )	opt->text(TSYS::real2str((float)sysClk()/1000000.));
    else if( a_path == "/gen/clk_res" && ctrChkNode(opt) )	
    {
        struct timespec tmval;
        clock_getres(CLOCK_REALTIME,&tmval);
        opt->text(TSYS::real2str((float)tmval.tv_nsec/1000000.));
    }
    else if( a_path == "/gen/in_charset" && ctrChkNode(opt) )	opt->text(Mess->charset());
    else if( a_path == "/gen/config" && ctrChkNode(opt) )	opt->text(m_confFile);
    else if( a_path == "/gen/wrk_db" )
    { 
	if( ctrChkNode(opt,"get",0660,"root",db().at().subId().c_str(),SEQ_RD) ) opt->text(mWorkDB); 
	if( ctrChkNode(opt,"set",0660,"root",db().at().subId().c_str(),SEQ_WR) ) mWorkDB = opt->text();
    }
    else if( a_path == "/gen/saveExit" )
    {
	if( ctrChkNode(opt,"get",0664,"root",db().at().subId().c_str(),SEQ_RD) ) opt->text(int2str(mSaveAtExit)); 
	if( ctrChkNode(opt,"set",0664,"root",db().at().subId().c_str(),SEQ_WR) ) mSaveAtExit = atoi(opt->text().c_str());
    }
    else if( a_path == "/gen/workdir" ) 
    {
	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->text(getcwd(buf,sizeof(buf)));
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )	chdir(opt->text().c_str());
    }
    else if( a_path == "/gen/lang" )
    {
	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->text(Mess->lang());
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )	Mess->lang(opt->text());
    }
    else if( a_path == "/gen/mess/lev" ) 
    {
	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->text(TSYS::int2str(Mess->messLevel()));
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )	Mess->messLevel(atoi(opt->text().c_str()));
    }
    else if( a_path == "/gen/mess/log_sysl" )
    {
    	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->text((Mess->logDirect()&0x01)?"1":"0");
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )	Mess->logDirect( atoi(opt->text().c_str())?Mess->logDirect()|0x01:Mess->logDirect()&(~0x01) );
    }
    else if( a_path == "/gen/mess/log_stdo" )	
    {
	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->text((Mess->logDirect()&0x02)?"1":"0");
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )	Mess->logDirect( atoi(opt->text().c_str())?Mess->logDirect()|0x02:Mess->logDirect()&(~0x02) );
    }
    else if( a_path == "/gen/mess/log_stde" )
    {
    	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->text((Mess->logDirect()&0x04)?"1":"0");
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )	Mess->logDirect( atoi(opt->text().c_str())?Mess->logDirect()|0x04:Mess->logDirect()&(~0x04) );
    }
    else if( a_path == "/gen/mess/log_arch" )
    {
	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->text((Mess->logDirect()&0x08)?"1":"0");
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )	Mess->logDirect( atoi(opt->text().c_str())?Mess->logDirect()|0x08:Mess->logDirect()&(~0x08) );
    }
    else if( a_path == "/subs/br" && ctrChkNode(opt,"get",0444,"root","root",SEQ_RD) )
    {
        vector<string> lst;
        list(lst);
        for( unsigned i_a=0; i_a < lst.size(); i_a++ )
	    opt->childAdd("el")->attr("id",lst[i_a])->text(at(lst[i_a]).at().subName());
    }
    else if( a_path == "/hlp/g_help" && ctrChkNode(opt,"get",0440,"root","root",SEQ_RD) )	opt->text(optDescr());
    else if( a_path == "/gen/load" && ctrChkNode(opt,"set",0660,"root","root",SEQ_WR) )		load();
    else if( a_path == "/gen/save" && ctrChkNode(opt,"set",0660,"root","root",SEQ_WR) )		save();
}
