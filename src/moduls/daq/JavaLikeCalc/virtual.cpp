
//OpenSCADA system module DAQ.JavaLikeCalc file: virtual.cpp
/***************************************************************************
 *   Copyright (C) 2005-2010 by Roman Savochenko                           *
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

#include <signal.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/times.h>

#include <tsys.h>
#include <tmess.h>
#include <ttiparam.h>

#include "freefunc.h"
#include "virtual.h"

//*************************************************
//* Modul info!                                   *
#define MOD_ID		"JavaLikeCalc"
#define MOD_NAME	_("Java-like based calculator")
#define MOD_TYPE	SDAQ_ID
#define VER_TYPE	SDAQ_VER
#define SUB_TYPE	"LIB"
#define VERSION		"1.8.0"
#define AUTORS		_("Roman Savochenko")
#define DESCRIPTION	_("Allow java-like based calculator and function's libraries engine. User can create and modify function and libraries.")
#define LICENSE		"GPL2"
//*************************************************

JavaLikeCalc::TipContr *JavaLikeCalc::mod;

extern "C"
{
    TModule::SAt module( int n_mod )
    {
	if( n_mod==0 )	return TModule::SAt(MOD_ID,MOD_TYPE,VER_TYPE);
	return TModule::SAt("");
    }

    TModule *attach( const TModule::SAt &AtMod, const string &source )
    {
	if( AtMod == TModule::SAt(MOD_ID,MOD_TYPE,VER_TYPE) )
	    return new JavaLikeCalc::TipContr( source );
	return NULL;
    }
}

using namespace JavaLikeCalc;

//*************************************************
//* TipContr                                      *
//*************************************************
TipContr::TipContr( string src ) : TTipDAQ(MOD_ID)
{
    mod		= this;

    mName	= MOD_NAME;
    mType	= MOD_TYPE;
    mVers	= VERSION;
    mAutor	= AUTORS;
    mDescr	= DESCRIPTION;
    mLicense	= LICENSE;
    mSource	= src;

    mLib = grpAdd("lib_");
}

TipContr::~TipContr()
{
    nodeDelAll();
}

void TipContr::postEnable( int flag )
{
    TTipDAQ::postEnable( flag );

    //> Controller db structure
    fldAdd( new TFld("PRM_BD",_("Parameters table"),TFld::String,TFld::NoFlag,"60","system") );
    fldAdd( new TFld("FUNC",_("Controller's function"),TFld::String,TFld::NoFlag,"40") );
    fldAdd( new TFld("SCHEDULE",_("Calc schedule"),TFld::String,TFld::NoFlag,"100","1") );
    fldAdd( new TFld("PRIOR",_("Calc task priority"),TFld::Integer,TFld::NoFlag,"2","0","-1;99") );
    fldAdd( new TFld("ITER",_("Iteration number in single calc"),TFld::Integer,TFld::NoFlag,"2","1","1;99") );

    //> Controller value db structure
    val_el.fldAdd( new TFld("ID",_("IO ID"),TFld::String,TCfg::Key,"20") );
    val_el.fldAdd( new TFld("VAL",_("IO value"),TFld::String,TFld::NoFlag,"50") );

    //> Add parameter types
    int t_prm = tpParmAdd("std","PRM_BD",_("Standard"));
    tpPrmAt(t_prm).fldAdd( new TFld("FLD",_("Data fields"),TFld::String,TFld::FullText|TCfg::NoVal,"300") );

    //> Lib's db structure
    lb_el.fldAdd( new TFld("ID",_("ID"),TFld::String,TCfg::Key,"20") );
    lb_el.fldAdd( new TFld("NAME",_("Name"),TFld::String,TCfg::TransltText,"50") );
    lb_el.fldAdd( new TFld("DESCR",_("Description"),TFld::String,TCfg::TransltText,"300") );
    lb_el.fldAdd( new TFld("DB",_("Data base"),TFld::String,TFld::NoFlag,"30") );
    lb_el.fldAdd( new TFld("PROG_TR",_("Program's text translation"),TFld::Boolean,TFld::NoFlag,"1","1") );

    //> Function's structure
    fnc_el.fldAdd( new TFld("ID",_("ID"),TFld::String,TCfg::Key,"20") );
    fnc_el.fldAdd( new TFld("NAME",_("Name"),TFld::String,TCfg::TransltText,"50") );
    fnc_el.fldAdd( new TFld("DESCR",_("Description"),TFld::String,TCfg::TransltText,"300") );
    fnc_el.fldAdd( new TFld("MAXCALCTM",_("Maximum calc time"),TFld::Integer,TFld::NoFlag,"3","10","0;999") );
    fnc_el.fldAdd( new TFld("FORMULA",_("Formula"),TFld::String,TCfg::TransltText,"10000") );

    //> Function's IO structure
    fncio_el.fldAdd( new TFld("F_ID",_("Function ID"),TFld::String,TCfg::Key,"20") );
    fncio_el.fldAdd( new TFld("ID",_("ID"),TFld::String,TCfg::Key,"20") );
    fncio_el.fldAdd( new TFld("NAME",_("Name"),TFld::String,TCfg::TransltText,"50") );
    fncio_el.fldAdd( new TFld("TYPE",_("Type"),TFld::Integer,TFld::NoFlag,"1") );
    fncio_el.fldAdd( new TFld("MODE",_("Mode"),TFld::Integer,TFld::NoFlag,"1") );
    fncio_el.fldAdd( new TFld("DEF",_("Default value"),TFld::String,TCfg::TransltText,"20") );
    fncio_el.fldAdd( new TFld("HIDE",_("Hide"),TFld::Boolean,TFld::NoFlag,"1") );
    fncio_el.fldAdd( new TFld("POS",_("Position"),TFld::Integer,TFld::NoFlag,"3") );

    //> Init named constant table
    double rvl;
    rvl = 3.14159265358l; mConst.push_back(NConst(TFld::Real,"pi",string((char*)&rvl,sizeof(rvl))));
    rvl = 2.71828182845l; mConst.push_back(NConst(TFld::Real,"e",string((char*)&rvl,sizeof(rvl))));
    rvl = EVAL_REAL; mConst.push_back(NConst(TFld::Real,"EVAL_REAL",string((char*)&rvl,sizeof(rvl))));
    int ivl;
    ivl = EVAL_INT; mConst.push_back(NConst(TFld::Integer,"EVAL_INT",string((char*)&ivl,sizeof(ivl))));
    ivl = EVAL_BOOL; mConst.push_back(NConst(TFld::Boolean,"EVAL_BOOL",string((char*)&ivl,1)));

    mConst.push_back(NConst(TFld::String,"EVAL_STR",EVAL_STR));

    //> Init buildin functions list
    mBFunc.push_back(BFunc("sin",Reg::FSin,1));
    mBFunc.push_back(BFunc("cos",Reg::FCos,1));
    mBFunc.push_back(BFunc("tan",Reg::FTan,1));
    mBFunc.push_back(BFunc("sinh",Reg::FSinh,1));
    mBFunc.push_back(BFunc("cosh",Reg::FCosh,1));
    mBFunc.push_back(BFunc("tanh",Reg::FTanh,1));
    mBFunc.push_back(BFunc("asin",Reg::FAsin,1));
    mBFunc.push_back(BFunc("acos",Reg::FAcos,1));
    mBFunc.push_back(BFunc("atan",Reg::FAtan,1));
    mBFunc.push_back(BFunc("rand",Reg::FRand,1));
    mBFunc.push_back(BFunc("lg",Reg::FLg,1));
    mBFunc.push_back(BFunc("ln",Reg::FLn,1));
    mBFunc.push_back(BFunc("exp",Reg::FExp,1));
    mBFunc.push_back(BFunc("pow",Reg::FPow,2));
    mBFunc.push_back(BFunc("min",Reg::FMin,2));
    mBFunc.push_back(BFunc("max",Reg::FMax,2));
    mBFunc.push_back(BFunc("sqrt",Reg::FSqrt,1));
    mBFunc.push_back(BFunc("abs",Reg::FAbs,1));
    mBFunc.push_back(BFunc("sign",Reg::FSign,1));
    mBFunc.push_back(BFunc("ceil",Reg::FCeil,1));
    mBFunc.push_back(BFunc("floor",Reg::FFloor,1));
}

TController *TipContr::ContrAttach( const string &name, const string &daq_db )
{
    return new Contr(name,daq_db,this);
}

bool TipContr::compileFuncLangs( vector<string> *ls )
{
    if( ls )
    {
	ls->clear();
	ls->push_back("JavaScript");
    }
    return true;
}

void TipContr::compileFuncSynthHighl( const string &lang, XMLNode &shgl )
{
    if(lang == "JavaScript")
    {
	//shgl.childAdd("rule")->setAttr("expr","(\\=|\\+|\\-|\\>|\\<|\\*|\\;|\\,)")->setAttr("color","darkblue")->setAttr("font_weight","1");
	shgl.childAdd("rule")->setAttr("expr","\\b(if|else|for|while|in|using|new|var|break|continue|return|Array|Object)\\b")->setAttr("color","darkblue")->setAttr("font_weight","1");
	shgl.childAdd("rule")->setAttr("expr","(\\?|\\:)")->setAttr("color","darkblue")->setAttr("font_weight","1");
	shgl.childAdd("rule")->setAttr("expr","(\\b0[xX][0-9a-fA-F]*\\b|\\b[+-]?[0-9]*\\.?[0-9]+[eE]?[-+]?[0-9]*\\b|\\btrue\\b|\\bfalse\\b)")->setAttr("color","blue");
	shgl.childAdd("rule")->setAttr("expr","\"[^\"]*\"")->setAttr("color","darkgreen");
	//shgl.childAdd("rule")->setAttr("expr","\".+?[^\\\\]\"")->setAttr("color","darkgreen");
	shgl.childAdd("rule")->setAttr("expr","//[^\n]*")->setAttr("color","gray")->setAttr("font_italic","1");
	//shgl.childAdd("blk")->setAttr("beg","\"")->setAttr("end","\"")->setAttr("color","darkgreen");
	shgl.childAdd("blk")->setAttr("beg","/\\*")->setAttr("end","\\*/")->setAttr("color","gray")->setAttr("font_italic","1");
    }
}

string TipContr::compileFunc( const string &lang, TFunction &fnc_cfg, const string &prog_text, const string &usings )
{
    if( lang != "JavaScript" )	throw TError(nodePath().c_str(),_("Compilation with the help of the program language %s is not supported."),lang.c_str());
    if( !lbPresent("sys_compile") ) lbReg( new Lib("sys_compile","","") );
    if( !lbAt("sys_compile").at().present(fnc_cfg.id()) ) lbAt("sys_compile").at().add(fnc_cfg.id().c_str(),"");

    AutoHD<Func> func = lbAt("sys_compile").at().at(fnc_cfg.id());

    bool isStart = func.at().startStat();
    try
    {
	((TFunction&)func.at()).operator=(fnc_cfg);
	if( func.at().startStat() && prog_text == func.at().prog() ) return func.at().nodePath();
    }
    catch(TError err) { if( isStart ) func.at().setStart(true); throw; }
    func.at().setProg(prog_text.c_str());
    try
    {
	if( func.at().startStat() ) func.at().setStart(false);
	func.at().setUsings(usings);
	func.at().setStart(true);
    }
    catch(TError err)
    {
	if( !func.at().use() )
	{
	    func.free();
	    lbAt("sys_compile").at().del(fnc_cfg.id().c_str());
	}
	throw TError(nodePath().c_str(),_("Compile error: %s\n"),err.mess.c_str());
    }
    return func.at().nodePath();
}

void TipContr::load_( )
{
    //> Load parameters from command line

    //> Load parameters

    //> Load function's libraries
    try
    {
	//>> Search and create new libraries
	TConfig c_el(&elLib());
	c_el.cfgViewAll(false);
	vector<string> db_ls;

	//>> Search into DB
	SYS->db().at().dbList(db_ls,true);
	for( int i_db = 0; i_db < db_ls.size(); i_db++ )
	    for( int lib_cnt = 0; SYS->db().at().dataSeek(db_ls[i_db]+"."+libTable(),"",lib_cnt++,c_el); )
	    {
		string l_id = c_el.cfg("ID").getS();
		if(!lbPresent(l_id)) lbReg(new Lib(l_id.c_str(),"",(db_ls[i_db]==SYS->workDB())?"*.*":db_ls[i_db]));
	    }

	//>> Search into config file
	if( SYS->chkSelDB("<cfg>") )
	    for( int lib_cnt = 0; SYS->db().at().dataSeek("",nodePath()+"lib/",lib_cnt++,c_el); )
	    {
		string l_id = c_el.cfg("ID").getS();
		if(!lbPresent(l_id)) lbReg(new Lib(l_id.c_str(),"","*.*"));
	    }
    }catch( TError err )
    {
	mess_err(err.cat.c_str(),"%s",err.mess.c_str());
	mess_err(nodePath().c_str(),_("Load function's libraries error.")); 
    }
}

void TipContr::modStart( )
{
    vector<string> lst;

    //> Start functions
    lbList(lst);
    for(int i_lb=0; i_lb < lst.size(); i_lb++ )
	lbAt(lst[i_lb]).at().setStart(true);

    TTipDAQ::modStart( );
}

void TipContr::modStop( )
{
    //> Stop and disable all JavaLike-controllers
    vector<string> lst;
    list(lst);
    for(int i_l=0; i_l<lst.size(); i_l++)
	at(lst[i_l]).at().disable( );

    //> Stop functions
    lbList(lst);
    for(int i_lb=0; i_lb < lst.size(); i_lb++ )
	lbAt(lst[i_lb]).at().setStart(false);
}

void TipContr::cntrCmdProc( XMLNode *opt )
{
    //> Get page info
    if(opt->name() == "info")
    {
	TTipDAQ::cntrCmdProc(opt);
	ctrMkNode("grp",opt,-1,"/br/lib_",_("Library"),RWRWR_,"root",SDAQ_ID,2,"idm","1","idSz","20");
	if(ctrMkNode("area",opt,1,"/libs",_("Functions' Libraries")))
	    ctrMkNode("list",opt,-1,"/libs/lb",_("Libraries"),RWRWR_,"root",SDAQ_ID,5,"tp","br","idm","1","s_com","add,del","br_pref","lib_","idSz","20");
	return;
    }

    //> Process command to page
    string a_path = opt->attr("path");
    if(a_path == "/br/lib_" || a_path == "/libs/lb")
    {
	if(ctrChkNode(opt,"get",RWRWR_,"root",SDAQ_ID,SEC_RD))
	{
	    vector<string> lst;
	    lbList(lst);
	    for(unsigned i_a=0; i_a < lst.size(); i_a++)
		opt->childAdd("el")->setAttr("id",lst[i_a])->setText(lbAt(lst[i_a]).at().name());
	}
	if(ctrChkNode(opt,"add",RWRWR_,"root",SDAQ_ID,SEC_WR))	lbReg(new Lib(TSYS::strEncode(opt->attr("id"),TSYS::oscdID).c_str(),opt->text().c_str(),"*.*"));
	if(ctrChkNode(opt,"del",RWRWR_,"root",SDAQ_ID,SEC_WR))	lbUnreg(opt->attr("id"),1);
    }
    else TTipDAQ::cntrCmdProc(opt);
}

NConst *TipContr::constGet( const char *nm )
{
    for( int i_cst = 0; i_cst < mConst.size(); i_cst++)
	if( mConst[i_cst].name == nm ) return &mConst[i_cst];
	    return NULL;
}

BFunc *TipContr::bFuncGet( const char *nm )
{
    for( int i_bf = 0; i_bf < mBFunc.size(); i_bf++)
	if( mBFunc[i_bf].name == nm ) return &mBFunc[i_bf];
	    return NULL;
}

//*************************************************
//* Contr: Controller object                      *
//*************************************************
Contr::Contr( string name_c, const string &daq_db, ::TElem *cfgelem) :
    ::TController(name_c, daq_db, cfgelem), TValFunc(name_c.c_str(),NULL,false), prc_st(false), endrun_req(false),
    mSched(cfg("SCHEDULE").getSd()), mPrior(cfg("PRIOR").getId()),
    mIter(cfg("ITER").getId()), mFnc(cfg("FUNC").getSd())
{
    cfg("PRM_BD").setS("JavaLikePrm_"+name_c);
    setDimens(true);
}

Contr::~Contr()
{

}

void Contr::postDisable(int flag)
{
    try
    {
	if( flag )
	{
	    //> Delete IO value's table
	    string db = DB()+"."+TController::id()+"_val";
	    SYS->db().at().open(db);
	    SYS->db().at().close(db,true);
	}
    }catch(TError err)
    { mess_err(nodePath().c_str(),"%s",err.mess.c_str()); }

    TController::postDisable(flag);
}

string Contr::getStatus( )
{
    string val = TController::getStatus( );

    if( startStat( ) && !redntUse( ) )
    {
	if( period() ) val += TSYS::strMess(_("Call by period %g s. "),(1e-9*period()));
	else val += TSYS::strMess(_("Call by cron '%s'. "),cron().c_str());
	val += TSYS::strMess(_("Calc time %.6g mks. "),calcTm());
    }

    return val;
}

void Contr::enable_( )
{
    if( !mod->lbPresent(TSYS::strSepParse(mFnc,0,'.')) )
	throw TError(nodePath().c_str(),_("Functions library '%s' is not present. Please, create functions library!"),TSYS::strSepParse(mFnc,0,'.').c_str());
    if( !mod->lbAt(TSYS::strSepParse(mFnc,0,'.')).at().present(TSYS::strSepParse(mFnc,1,'.')) )
    {
	mess_info(nodePath().c_str(),_("Create new function '%s'."),mFnc.c_str());
	mod->lbAt(TSYS::strSepParse(mFnc,0,'.')).at().add(TSYS::strSepParse(mFnc,1,'.').c_str());
    }
    setFunc( &mod->lbAt(TSYS::strSepParse(mFnc,0,'.')).at().at(TSYS::strSepParse(mFnc,1,'.')).at() );
    try{ loadFunc( ); }
    catch(TError err)
    {
	mess_warning(err.cat.c_str(),"%s",err.mess.c_str());
	mess_warning(nodePath().c_str(),_("Load function and its io error."));
    }
}

void Contr::disable_( )
{
    setFunc(NULL);
}

void Contr::load_( )
{
    TController::load_( );

    loadFunc( );
}

void Contr::loadFunc( bool onlyVl )
{
    if( func() != NULL )
    {
	if( !onlyVl ) ((Func *)func())->load();

	//> Creating special IO
	if( func()->ioId("f_frq") < 0 ) func()->ioIns( new IO("f_frq",_("Function calculate frequency (Hz)"),IO::Real,Func::SysAttr,"1000",false),0);
	if( func()->ioId("f_start") < 0 ) func()->ioIns( new IO("f_start",_("Function start flag"),IO::Boolean,Func::SysAttr,"0",false),1);
	if( func()->ioId("f_stop") < 0 ) func()->ioIns( new IO("f_stop",_("Function stop flag"),IO::Boolean,Func::SysAttr,"0",false),2);

	//> Load values
	TConfig cfg(&mod->elVal());
	string bd_tbl = TController::id()+"_val";
	string bd = DB()+"."+bd_tbl;

	for( int fld_cnt=0; SYS->db().at().dataSeek(bd,mod->nodePath()+bd_tbl,fld_cnt++,cfg); )
	{
	    int ioId = func()->ioId(cfg.cfg("ID").getS());
	    if( ioId < 0 || func()->io(ioId)->flg()&Func::SysAttr ) continue;
	    setS(ioId,cfg.cfg("VAL").getS());
	}
    }
}

void Contr::postIOCfgChange( )
{
    TValFunc::postIOCfgChange( );

    loadFunc( true );
}

void Contr::save_( )
{
    TController::save_();

    if( func() != NULL )
    {
	((Func *)func())->save();

	//> Save values
	TConfig cfg(&mod->elVal());
	string bd_tbl = TController::id()+"_val";
	string val_bd = DB()+"."+bd_tbl;
	for( int iio = 0; iio < ioSize(); iio++ )
	{
	    if( func()->io(iio)->flg()&Func::SysAttr ) continue;
	    cfg.cfg("ID").setS(func()->io(iio)->id());
	    cfg.cfg("VAL").setS(getS(iio));
	    SYS->db().at().dataSet(val_bd,mod->nodePath()+bd_tbl,cfg);
	}

	//> Clear VAL
	cfg.cfgViewAll(false);
	for( int fld_cnt=0; SYS->db().at().dataSeek(val_bd,mod->nodePath()+bd_tbl,fld_cnt++,cfg); )
	    if( ioId(cfg.cfg("ID").getS()) < 0 )
	    {
		SYS->db().at().dataDel(val_bd,mod->nodePath()+bd_tbl,cfg,true);
		fld_cnt--;
	    }
    }
}

void Contr::start_( )
{
    ((Func *)func())->setStart( true );

    //> Schedule process
    mPer = TSYS::strSepParse(mSched,1,' ').empty() ? vmax(0,(long long)(1e9*atof(mSched.c_str()))) : 0;

    //> Start the request data task
    if( !prc_st ) SYS->taskCreate( nodePath('.',true), mPrior, Contr::Task, this, &prc_st );
}

void Contr::stop_( )
{
    //> Stop the request and calc data task
    if( prc_st ) SYS->taskDestroy( nodePath('.',true), &prc_st, &endrun_req );
}

void *Contr::Task( void *icntr )
{
    Contr &cntr = *(Contr *)icntr;

    cntr.endrun_req = false;
    cntr.prc_st = true;

    bool is_start = true;
    bool is_stop  = false;

    while(true)
    {
	if(!cntr.redntUse())
	{
	    //> Setting special IO
	    int ioI = cntr.ioId("f_frq");	if(ioI >= 0) cntr.setR(ioI,cntr.period()?(float)cntr.iterate()*1e9/(float)cntr.period():0);
	    ioI = cntr.ioId("f_start");		if(ioI >= 0) cntr.setB(ioI,is_start);
	    ioI = cntr.ioId("f_stop");		if(ioI >= 0) cntr.setB(ioI,is_stop);

	    for(int i_it = 0; i_it < cntr.mIter; i_it++)
		try { cntr.calc(); }
		catch(TError err)
		{
		    mess_err(err.cat.c_str(),"%s",err.mess.c_str() ); 
		    mess_err(cntr.nodePath().c_str(),_("Calc controller's function error."));
		}
	}

	if(is_stop) break;
	TSYS::taskSleep(cntr.period(),cntr.period()?0:TSYS::cron(cntr.cron()));
	if(cntr.endrun_req) is_stop = true;
	is_start = false;
	cntr.modif();
    }

    cntr.prc_st = false;

    return NULL;
}

void Contr::redntDataUpdate( )
{
    TController::redntDataUpdate( );

    //> Request for template's attributes values
    XMLNode req("get"); req.setAttr("path",nodePath(0,true)+"/%2fserv%2ffncAttr");

    //> Send request to first active station for this controller
    if( owner().owner().rdStRequest(workId(),req).empty() ) return;

    //> Redirect respond to local controller
    req.setName("set")->setAttr("path","/%2fserv%2ffncAttr");
    cntrCmd(&req);
}


TParamContr *Contr::ParamAttach( const string &name, int type )
{
    return new Prm(name,&owner().tpPrmAt(type));
}

void Contr::cntrCmdProc( XMLNode *opt )
{
    //> Service commands process
    string a_path = opt->attr("path");
    if(a_path.substr(0,6) == "/serv/")
    {
	if(a_path == "/serv/fncAttr")
	{
	    if(!startStat() || !func()) throw TError(nodePath().c_str(),_("No started or no present function."));
	    if(ctrChkNode(opt,"get",RWRWR_,"root",SDAQ_ID,SEC_RD))
		for(int i_a = 0; i_a < ioSize(); i_a++)
		    opt->childAdd("a")->setAttr("id",func()->io(i_a)->id())->setText(getS(i_a));
	    if(ctrChkNode(opt,"set",RWRWR_,"root",SDAQ_ID,SEC_WR))
		for(int i_a = 0; i_a < opt->childSize(); i_a++)
		{
		    int io_id = -1;
		    if(opt->childGet(i_a)->name() != "a" || (io_id=ioId(opt->childGet(i_a)->attr("id"))) < 0) continue;
		    setS(io_id,opt->childGet(i_a)->text());
		}
	}
	else TController::cntrCmdProc(opt);
	return;
    }

    //> Get page info
    if(opt->name() == "info")
    {
	TController::cntrCmdProc(opt);
	ctrMkNode("fld",opt,-1,"/cntr/cfg/FUNC",cfg("FUNC").fld().descr(),RWRWR_,"root",SDAQ_ID,3,"tp","str","dest","sel_ed","select","/cntr/flst");
	ctrMkNode("fld",opt,-1,"/cntr/cfg/SCHEDULE",cfg("SCHEDULE").fld().descr(),RWRWR_,"root",SDAQ_ID,4,"tp","str","dest","sel_ed",
	    "sel_list","1;1e-3;* * * * *;10 * * * *;10-20 2 */2 * *",
	    "help",_("Schedule is writed in seconds periodic form or in standard Cron form.\n"
		     "Seconds form is one real number (1.5, 1e-3).\n"
		     "Cron it is standard form '* * * * *'. Where:\n"
		     "  - minutes (0-59);\n"
		     "  - hours (0-23);\n"
		     "  - days (1-31);\n"
		     "  - month (1-12);\n"
		     "  - week day (0[sunday]-6)."));
	if(enableStat() && ctrMkNode("area",opt,-1,"/fnc",_("Calcing")))
	{
	    if(ctrMkNode("table",opt,-1,"/fnc/io",_("Data"),RWRWR_,"root",SDAQ_ID,2,"s_com","add,del,ins,move","rows","15"))
	    {
		ctrMkNode("list",opt,-1,"/fnc/io/0",_("Id"),RWRWR_,"root",SDAQ_ID,1,"tp","str");
		ctrMkNode("list",opt,-1,"/fnc/io/1",_("Name"),RWRWR_,"root",SDAQ_ID,1,"tp","str");
		ctrMkNode("list",opt,-1,"/fnc/io/2",_("Type"),RWRWR_,"root",SDAQ_ID,5,"tp","dec","idm","1","dest","select",
		    "sel_id",(TSYS::int2str(IO::Real)+";"+TSYS::int2str(IO::Integer)+";"+TSYS::int2str(IO::Boolean)+";"+TSYS::int2str(IO::String)).c_str(),
		    "sel_list",_("Real;Integer;Boolean;String"));
		ctrMkNode("list",opt,-1,"/fnc/io/3",_("Mode"),RWRWR_,"root",SDAQ_ID,5,"tp","dec","idm","1","dest","select",
		    "sel_id",(TSYS::int2str(IO::Default)+";"+TSYS::int2str(IO::Output)+";"+TSYS::int2str(IO::Return)).c_str(),
		    "sel_list",_("Input;Output;Return"));
		ctrMkNode("list",opt,-1,"/fnc/io/4",_("Value"),RWRWR_,"root",SDAQ_ID,1,"tp","str");
	    }
	    ctrMkNode("fld",opt,-1,"/fnc/prog",_("Programm"),RWRW__,"root",SDAQ_ID,3,"tp","str","rows","10","SnthHgl","1");
	}
	return;
    }

    //> Process command to page
    if(a_path == "/cntr/flst" && ctrChkNode(opt))
    {
	vector<string> lst;
	int c_lv = 0;
	string c_path = "", c_el;
	opt->childAdd("el")->setText(c_path);
	for(int c_off = 0; (c_el=TSYS::strSepParse(mFnc,0,'.',&c_off)).size(); c_lv++)
	{
	    c_path += c_lv ? "."+c_el : c_el;
	    opt->childAdd("el")->setText(c_path);
	}
	if(c_lv) c_path+=".";
	switch(c_lv)
	{
	    case 0:	mod->lbList(lst); break;
	    case 1:
		if(mod->lbPresent(TSYS::strSepParse(mFnc,0,'.')))
		    mod->lbAt(TSYS::strSepParse(mFnc,0,'.')).at().list(lst);
		break;
	}
	for(unsigned i_a=0; i_a < lst.size(); i_a++)
	    opt->childAdd("el")->setText(c_path+lst[i_a]);
    }
    else if(a_path == "/fnc/io" && enableStat())
    {
	if(ctrChkNode(opt,"get",RWRWR_,"root",SDAQ_ID,SEC_RD))
	{
	    XMLNode *n_id	= ctrMkNode("list",opt,-1,"/fnc/io/0","",RWRWR_);
	    XMLNode *n_nm	= ctrMkNode("list",opt,-1,"/fnc/io/1","",RWRWR_);
	    XMLNode *n_type	= ctrMkNode("list",opt,-1,"/fnc/io/2","",RWRWR_);
	    XMLNode *n_mode	= ctrMkNode("list",opt,-1,"/fnc/io/3","",RWRWR_);
	    XMLNode *n_val	= ctrMkNode("list",opt,-1,"/fnc/io/4","",RWRWR_);

	    for(int id = 0; id < func()->ioSize(); id++)
	    {
		if(n_id)	n_id->childAdd("el")->setText(func()->io(id)->id());
		if(n_nm)	n_nm->childAdd("el")->setText(func()->io(id)->name());
		if(n_type)	n_type->childAdd("el")->setText(TSYS::int2str(func()->io(id)->type()));
		if(n_mode)	n_mode->childAdd("el")->setText(TSYS::int2str(func()->io(id)->flg()&(IO::Output|IO::Return)));
		if(n_val)	n_val->childAdd("el")->setText(getS(id));
	    }
	}
	if(ctrChkNode(opt,"add",RWRWR_,"root",SDAQ_ID,SEC_WR))
	{ ((Func *)func())->ioAdd( new IO("new","New IO",IO::Real,IO::Default) ); modif(); }
	if(ctrChkNode(opt,"ins",RWRWR_,"root",SDAQ_ID,SEC_WR))
	{ ((Func *)func())->ioIns( new IO("new","New IO",IO::Real,IO::Default), atoi(opt->attr("row").c_str()) ); modif(); }
	if(ctrChkNode(opt,"del",RWRWR_,"root",SDAQ_ID,SEC_WR))
	{
	    int row = atoi(opt->attr("row").c_str());
	    if(func()->io(row)->flg()&Func::SysAttr)
		throw TError(nodePath().c_str(),_("Deleting lock attribute in not allow."));
	    ((Func *)func())->ioDel(row); 
	    modif();
	}
	if(ctrChkNode(opt,"move",RWRWR_,"root",SDAQ_ID,SEC_WR))
	{ ((Func *)func())->ioMove( atoi(opt->attr("row").c_str()), atoi(opt->attr("to").c_str()) ); modif(); }
	if(ctrChkNode(opt,"set",RWRWR_,"root",SDAQ_ID,SEC_WR))
	{
	    int row = atoi(opt->attr("row").c_str());
	    int col = atoi(opt->attr("col").c_str());
	    if((col == 0 || col == 1) && !opt->text().size())
		throw TError(nodePath().c_str(),_("Empty value is not valid."));
	    if(func()->io(row)->flg()&Func::SysAttr)
		throw TError(nodePath().c_str(),_("Changing locked attribute is not allowed."));
	    switch(col)
	    {
		case 0:	func()->io(row)->setId(opt->text());	break;
		case 1:	func()->io(row)->setName(opt->text());	break;
		case 2:	func()->io(row)->setType((IO::Type)atoi(opt->text().c_str()));	break;
		case 3:	func()->io(row)->setFlg( func()->io(row)->flg()^((atoi(opt->text().c_str())^func()->io(row)->flg())&(IO::Output|IO::Return)) );	break;
		case 4:	setS(row,opt->text());	break;
	    }
	    modif();
	    if(!((Func *)func())->owner().DB().empty()) ((Func *)func())->modif();
	}
    }
    else if(a_path == "/fnc/prog" && enableStat())
    {
	if(ctrChkNode(opt,"get",RWRW__,"root",SDAQ_ID,SEC_RD))	opt->setText(((Func *)func())->prog());
	if(ctrChkNode(opt,"set",RWRW__,"root",SDAQ_ID,SEC_WR))
	{
	    ((Func *)func())->setProg(opt->text().c_str());
	    ((Func *)func())->progCompile();
	    modif();
	}
	if(ctrChkNode(opt,"SnthHgl",RWRW__,"root",SDAQ_ID,SEC_RD))	mod->compileFuncSynthHighl("JavaScript",*opt);
    }
    else TController::cntrCmdProc(opt);
}

//*************************************************
//* Prm                                           *
//*************************************************
Prm::Prm( string name, TTipParam *tp_prm ) :
    TParamContr(name,tp_prm), v_el(name)
{

}

Prm::~Prm()
{
    nodeDelAll();
}

void Prm::postEnable( int flag )
{
    TParamContr::postEnable( flag );
    if(!vlElemPresent(&v_el)) vlElemAtt(&v_el);
}

void Prm::enable()
{
    if( enableStat() )  return;

    //> Check and delete no used fields
    for(int i_fld = 0; i_fld < v_el.fldSize(); i_fld++)
    {
	string fel;
	for( int io_off = 0; (fel=TSYS::strSepParse(cfg("FLD").getS(),0,'\n',&io_off)).size(); )
	    if( TSYS::strSepParse(fel,0,':') == v_el.fldAt(i_fld).reserve() ) break;
	if( fel.empty() )
	{
	    try{ v_el.fldDel(i_fld); i_fld--; }
	    catch(TError err)
	    { mess_err(err.cat.c_str(),"%s",err.mess.c_str()); }
	}
    }

    //> Init elements
    vector<string> pls;
    int io;
    string mio, ionm, aid, anm, def;
    for( int io_off = 0; (mio=TSYS::strSepParse(cfg("FLD").getS(),0,'\n',&io_off)).size(); )
    {
	ionm   = TSYS::strSepParse(mio,0,':');
	aid    = TSYS::strSepParse(mio,1,':');
	anm    = TSYS::strSepParse(mio,2,':');
	if( aid.empty() ) aid = ionm;

	int	io_id = ((Contr &)owner()).ioId(ionm);
	if( io_id < 0 )	continue;

	unsigned	flg = TVal::DirWrite|TVal::DirRead;
	if( !(((Contr &)owner()).ioFlg(io_id) & (IO::Output|IO::Return)) ) flg |= TFld::NoWrite;
	TFld::Type	tp  = TFld::String;
	switch( ((Contr &)owner()).ioType(io_id) )
	{
	    case IO::String:	tp = TFld::String; def = EVAL_STR;			break;
	    case IO::Integer:	tp = TFld::Integer; def = TSYS::int2str(EVAL_INT);	break;
	    case IO::Real:	tp = TFld::Real; def = TSYS::real2str(EVAL_REAL);	break;
	    case IO::Boolean:	tp = TFld::Boolean; def = TSYS::int2str(EVAL_BOOL);	break;
	}
	if( !v_el.fldPresent(aid) || v_el.fldAt(v_el.fldId(aid)).type() != tp || v_el.fldAt(v_el.fldId(aid)).flg() != flg )
	{
	    if( v_el.fldPresent(aid) ) v_el.fldDel(v_el.fldId(aid));
	    v_el.fldAdd( new TFld(aid.c_str(),"",tp,flg,"",def.c_str()) );
	}

	int el_id = v_el.fldId(aid);
	v_el.fldAt(el_id).setDescr( anm.empty() ? ((Contr &)owner()).func()->io(io_id)->name() : anm );
	v_el.fldAt(el_id).setReserve( ionm );

	pls.push_back(aid);
    }

    //> Check and delete no used attrs
    for( int i_fld = 0; i_fld < v_el.fldSize(); i_fld++ )
    {
	int i_p;
	for( i_p = 0; i_p < pls.size(); i_p++ )
	    if( pls[i_p] == v_el.fldAt(i_fld).name() )  break;
	if( i_p < pls.size() )  continue;
	try{ v_el.fldDel(i_fld); i_fld--; }
	catch( TError err ) { mess_err(err.cat.c_str(),"%s",err.mess.c_str()); }
    }

    TParamContr::enable();
}

void Prm::disable()
{
    if( !enableStat() )  return;

    TParamContr::disable();
}

Contr &Prm::owner( )	{ return (Contr&)TParamContr::owner(); }

void Prm::vlSet( TVal &val, const TVariant &pvl )
{
    if( !enableStat() ) return;

    //> Send to active reserve station
    if( owner().redntUse( ) )
    {
	if( val.getS(0,true) == pvl.getS() ) return;
	XMLNode req("set");
	req.setAttr("path",nodePath(0,true)+"/%2fserv%2fattr")->childAdd("el")->setAttr("id",val.name())->setText(val.getS(0,true));
	SYS->daq().at().rdStRequest(owner().workId(),req);
	return;
    }

    //> Direct write
    try
    {
	int io_id = ((Contr &)owner()).ioId(val.fld().reserve());
	if( io_id < 0 ) disable();
	else
	{
	    switch(val.fld().type())
	    {
		case TFld::String:
		    ((Contr &)owner()).setS(io_id,val.getS(0,true));
		    break;
		case TFld::Integer:
		    ((Contr &)owner()).setI(io_id,val.getI(0,true));
		    break;
		case TFld::Real:
		    ((Contr &)owner()).setR(io_id,val.getR(0,true));
		    break;
		case TFld::Boolean:
		    ((Contr &)owner()).setB(io_id,val.getB(0,true));
		    break;
	    }
	}
    }catch(TError err) { disable(); }
}

void Prm::vlGet( TVal &val )
{
    if( val.name() == "err" )
    {
	if( !owner().startStat() )	val.setS(_("2:Controller is stoped"),0,true);
	else if( !enableStat() )	val.setS(_("1:Parameter is disabled"),0,true);
	else val.setS("0",0,true);
	return;
    }
    if( owner().redntUse( ) ) return;
    try
    {
	int io_id = ((Contr &)owner()).ioId(val.fld().reserve());
	if( io_id < 0 ) disable();
	else
	{
	    switch(val.fld().type())
	    {
		case TFld::String:
		    val.setS(enableStat()?owner().getS(io_id):EVAL_STR,0,true);	break;
		case TFld::Integer:
		    val.setI(enableStat()?owner().getI(io_id):EVAL_INT,0,true);	break;
		case TFld::Real:
		    val.setR(enableStat()?owner().getR(io_id):EVAL_REAL,0,true);break;
		case TFld::Boolean:
		    val.setB(enableStat()?owner().getB(io_id):EVAL_BOOL,0,true);break;
	    }
        }
    }catch(TError err) { disable(); }
}

void Prm::vlArchMake( TVal &val )
{
    if( val.arch().freeStat() ) return;
    val.arch().at().setSrcMode(TVArchive::ActiveAttr,val.arch().at().srcData());
    val.arch().at().setPeriod( owner().period() ? owner().period()/1e3 : 1000000 );
    val.arch().at().setHardGrid( true );
    val.arch().at().setHighResTm( true );
}

void Prm::cntrCmdProc( XMLNode *opt )
{
    //Get page info
    if(opt->name() == "info")
    {
	TParamContr::cntrCmdProc(opt);
	ctrMkNode("fld",opt,-1,"/prm/cfg/FLD",cfg("FLD").fld().descr(),RWRWR_,"root",SDAQ_ID,1,
	    "help",_("Attributes configuration list. List must be written by lines in format: [<io>:<aid>:<anm>]\n"
	    "Where:\n"
	    "  io - calc function's IO;\n"
	    "  aid - created attribute identifier;\n"
	    "  anm - created attribute name.\n"
	    "If 'aid' or 'anm' are not set they will be generated from selected function's IO."));
	return;
    }
    //Process command to page
    TParamContr::cntrCmdProc(opt);
}
