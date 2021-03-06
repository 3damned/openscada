
//OpenSCADA system module DAQ.ModBus file: modbus_daq.cpp
/***************************************************************************
 *   Copyright (C) 2007-2017 by Roman Savochenko, <rom_as@oscada.org>      *
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdint.h>
#include <algorithm>

#include <ttypeparam.h>

#include "modbus_daq.h"

ModBus::TTpContr *ModBus::mod;

using namespace ModBus;

//******************************************************
//* TTpContr                                           *
//******************************************************
TTpContr::TTpContr( string name ) : TTypeDAQ(DAQ_ID)
{
    mod = this;

    modInfoMainSet(DAQ_NAME, DAQ_TYPE, DAQ_MVER, DAQ_AUTHORS, DAQ_DESCR, DAQ_LICENSE, name);
}

TTpContr::~TTpContr( )
{

}

void TTpContr::postEnable( int flag )
{
    TTypeDAQ::postEnable(flag);

    //Controler's bd structure
    fldAdd(new TFld("PRM_BD",_("Parameters table"),TFld::String,TFld::NoFlag,"30",""));
    fldAdd(new TFld("PRM_BD_L",_("Logical parameters table"),TFld::String,TFld::NoFlag,"30",""));
    fldAdd(new TFld("SCHEDULE",_("Acquisition schedule"),TFld::String,TFld::NoFlag,"100","1"));
    fldAdd(new TFld("PRIOR",_("Gather task priority"),TFld::Integer,TFld::NoFlag,"2","0","-1;199"));
    fldAdd(new TFld("PROT",_("Modbus protocol"),TFld::String,TFld::Selected,"5","TCP","TCP;RTU;ASCII",_("TCP/IP;RTU;ASCII")));
    fldAdd(new TFld("ADDR",_("Transport address"),TFld::String,TFld::NoFlag,"41",""));
    fldAdd(new TFld("NODE",_("Destination node"),TFld::Integer,TFld::NoFlag,"20","1","0;255"));
    fldAdd(new TFld("FRAG_MERGE",_("Data fragments merge"),TFld::Boolean,TFld::NoFlag,"1","0"));
    fldAdd(new TFld("WR_MULTI",_("Use multi-items write functions (15,16)"),TFld::Boolean,TFld::NoFlag,"1","0"));
    fldAdd(new TFld("WR_ASYNCH",_("Asynchronous write"),TFld::Boolean,TFld::NoFlag,"1","0"));
    fldAdd(new TFld("TM_REQ",_("Connection timeout (ms)"),TFld::Integer,TFld::NoFlag,"5","0","0;10000"));
    fldAdd(new TFld("TM_REST",_("Restore timeout (s)"),TFld::Integer,TFld::NoFlag,"4","30","1;3600"));
    fldAdd(new TFld("REQ_TRY",_("Request tries"),TFld::Integer,TFld::NoFlag,"1","1","1;9"));
    fldAdd(new TFld("MAX_BLKSZ",_("Maximum request block size (bytes)"),TFld::Integer,TFld::NoFlag,"3","200","2;250"));

    //Parameter type bd structure
    // Standard parameter type by symple attributes list
    int t_prm = tpParmAdd("std", "PRM_BD", _("Standard"), true);
    tpPrmAt(t_prm).fldAdd(new TFld("ATTR_LS",_("Attributes list"),TFld::String,TFld::FullText|TFld::TransltText|TCfg::NoVal,"100000",""));
    // Extended logical parameter type by DAQ parameter's template
    t_prm = tpParmAdd("logic","PRM_BD_L",_("Logical"));
    tpPrmAt(t_prm).fldAdd(new TFld("TMPL",_("Parameter template"),TFld::String,TCfg::NoVal,"50",""));
    //  Parameter template IO DB structure
    elPrmIO.fldAdd(new TFld("PRM_ID",_("Parameter ID"),TFld::String,TCfg::Key,i2s(atoi(OBJ_ID_SZ)*6).c_str()));
    elPrmIO.fldAdd(new TFld("ID",_("ID"),TFld::String,TCfg::Key,OBJ_ID_SZ));
    elPrmIO.fldAdd(new TFld("VALUE",_("Value"),TFld::String,TFld::NoFlag,"1000000"));
}

void TTpContr::load_( )
{
    //Load parameters from command line

}

void TTpContr::save_( )
{

}

TController *TTpContr::ContrAttach( const string &name, const string &daq_db )	{ return new TMdContr(name,daq_db,this); }

//******************************************************
//* TMdContr                                           *
//******************************************************
TMdContr::TMdContr(string name_c, const string &daq_db, TElem *cfgelem) :
	TController(name_c, daq_db, cfgelem),
	mPrior(cfg("PRIOR").getId()), mNode(cfg("NODE").getId()), blkMaxSz(cfg("MAX_BLKSZ").getId()),
	mSched(cfg("SCHEDULE")), mPrt(cfg("PROT")), mAddr(cfg("ADDR")),
	mMerge(cfg("FRAG_MERGE").getBd()), mMltWr(cfg("WR_MULTI").getBd()), mAsynchWr(cfg("WR_ASYNCH").getBd()),
	reqTm(cfg("TM_REQ").getId()), restTm(cfg("TM_REST").getId()), connTry(cfg("REQ_TRY").getId()),
	prcSt(false), callSt(false), endrunReq(false), isReload(false), alSt(-1),
	tmDelay(0), numRReg(0), numRRegIn(0), numRCoil(0), numRCoilIn(0), numWReg(0), numWCoil(0), numErrCon(0), numErrResp(0)
{
    cfg("PRM_BD").setS("ModBusPrm_"+name_c);
    cfg("PRM_BD_L").setS("ModBusPrmL_"+name_c);
    mPrt = "TCP";
}

TMdContr::~TMdContr( )
{
    if(startStat()) stop();
}

void TMdContr::postDisable( int flag )
{
    TController::postDisable(flag);
    try {
	if(flag) {
	    //Delete logical parameter's io table
	    string tbl = DB()+"."+cfg("PRM_BD_L").getS()+"_io";
	    SYS->db().at().open(tbl);
	    SYS->db().at().close(tbl,true);
	}
    } catch(TError &err) { mess_err(err.cat.c_str(),"%s",err.mess.c_str()); }
}

string TMdContr::getStatus( )
{
    string val = TController::getStatus( );

    if(startStat() && !redntUse()) {
	if(!prcSt) val += TSYS::strMess(_("Task terminated! "));
	if(tmDelay > -1) {
	    val += TSYS::strMess(_("Connection error. Restoring in %.6g s."), tmDelay);
	    val.replace(0, 1, "10");
	}
	else {
	    if(callSt)	val += TSYS::strMess(_("Call now. "));
	    if(period())val += TSYS::strMess(_("Call by period: %s. "), tm2s(1e-9*period()).c_str());
	    else val += TSYS::strMess(_("Call next by cron '%s'. "), atm2s(TSYS::cron(cron()),"%d-%m-%Y %R").c_str());
	    val += TSYS::strMess(_("Spent time: %s[%s]. Read %g(%g) registers, %g(%g) coils. Wrote %g registers, %g coils. Errors of connection %g, of respond %g."),
			tm2s(SYS->taskUtilizTm(nodePath('.',true))).c_str(), tm2s(SYS->taskUtilizTm(nodePath('.',true),true)).c_str(),
			numRReg,numRRegIn,numRCoil,numRCoilIn,numWReg,numWCoil,numErrCon,numErrResp);
	}
    }

    return val;
}

TParamContr *TMdContr::ParamAttach( const string &name, int type )	{ return new TMdPrm(name, &owner().tpPrmAt(type)); }

void TMdContr::disable_( )
{
    //Clear acquisition data block
    reqRes.resRequestW(true);
    acqBlks.clear();
    acqBlksIn.clear();
    acqBlksCoil.clear();
    acqBlksCoilIn.clear();
    reqRes.resRelease();
}

void TMdContr::start_( )
{
    if(prcSt) return;

    //Establish connection
    /*AutoHD<TTransportOut> tr = SYS->transport().at().at(TSYS::strParse(addr(),0,".")).at().outAt(TSYS::strParse(addr(),1,"."));
    try { tr.at().start(); }
    catch(TError &err) { mess_err(err.cat.c_str(),"%s",err.mess.c_str()); }*/

    //Schedule process
    mPer = TSYS::strSepParse(cron(),1,' ').empty() ? vmax(0,(int64_t)(1e9*s2r(cron()))) : 0;

    //Clear statistic
    numRReg = numRRegIn = numRCoil = numRCoilIn = numWReg = numWCoil = numErrCon = numErrResp = 0;
    tmDelay = 0;

    //Reenable parameters for data blocks structure update
    // Asynchronous writings queue clear
    dataRes().lock(); asynchWrs.clear(); dataRes().unlock();

    // Clear data blocks
    reqRes.resRequestW(true);
    acqBlks.clear();
    acqBlksIn.clear();
    acqBlksCoil.clear();
    acqBlksCoilIn.clear();
    reqRes.resRelease();

    // Reenable parameters
    try {
	vector<string> pls;
	list(pls);

	isReload = true;
	for(unsigned iP = 0; iP < pls.size(); iP++)
	    if(at(pls[iP]).at().enableStat()) at(pls[iP]).at().enable();
	isReload = false;
    } catch(TError&) { isReload = false; throw; }

    //Start the gathering data task
    SYS->taskCreate(nodePath('.',true), mPrior, TMdContr::Task, this);
}

void TMdContr::stop_( )
{
    //Stop the request and calc data task
    SYS->taskDestroy(nodePath('.',true), &endrunReq);

    alarmSet(TSYS::strMess(_("DAQ.%s.%s: connect to data source: %s."),owner().modId().c_str(),id().c_str(),_("STOP")), TMess::Info);
    alSt = -1;

    //Clear statistic
    numRReg = numRRegIn = numRCoil = numRCoilIn = numWReg = numWCoil = numErrCon = numErrResp = 0;

    //Clear process parameters list
    MtxAlloc res(enRes, true);
    pHd.clear();
}

bool TMdContr::cfgChange( TCfg &co, const TVariant &pc )
{
    TController::cfgChange(co, pc);

    if(co.fld().name() == "SCHEDULE" && startStat())
	mPer = TSYS::strSepParse(cron(),1,' ').empty() ? vmax(0,(int64_t)(1e9*s2r(cron()))) : 0;
    else if(co.fld().name() == "PROT") {
	cfg("REQ_TRY").setView(co.getS()!="TCP");
	if(startStat()) stop();
    }
    else if(co.fld().name() == "FRAG_MERGE" && enableStat()) disable();

    return true;
}

void TMdContr::prmEn( TMdPrm *prm, bool val )
{
    unsigned iPrm;

    MtxAlloc res(enRes, true);
    for(iPrm = 0; iPrm < pHd.size(); iPrm++)
	if(&pHd[iPrm].at() == prm) break;

    if(val && iPrm >= pHd.size())	pHd.push_back(prm);
    if(!val && iPrm < pHd.size())	pHd.erase(pHd.begin()+iPrm);
}

void TMdContr::regVal( int reg, const string &dt )
{
    if(reg < 0)	return;

    ResAlloc res(reqRes, true);

    //Register to acquisition block
    if(dt == "R" || dt == "RI") {
	vector< SDataRec > &workCnt = (dt == "RI") ? acqBlksIn : acqBlks;
	unsigned i_b;
	for(i_b = 0; i_b < workCnt.size(); i_b++) {
	    if((reg*2) < workCnt[i_b].off) {
		if((mMerge || (reg*2+2) >= workCnt[i_b].off) && (int)(workCnt[i_b].val.size()+workCnt[i_b].off-(reg*2)) < blkMaxSz)
		{
		    workCnt[i_b].val.insert(0,workCnt[i_b].off-reg*2,0);
		    workCnt[i_b].off = reg*2;
		}
		else workCnt.insert(workCnt.begin()+i_b,SDataRec(reg*2,2));
	    }
	    else if((reg*2+2) > (workCnt[i_b].off+(int)workCnt[i_b].val.size())) {
		if((mMerge || reg*2 <= (workCnt[i_b].off+(int)workCnt[i_b].val.size())) && (reg*2+2-workCnt[i_b].off) < blkMaxSz)
		{
		    workCnt[i_b].val.append((reg*2+2)-(workCnt[i_b].off+workCnt[i_b].val.size()),0);
		    // Check for allow mergin to next block
		    if(!mMerge && i_b+1 < workCnt.size() && (workCnt[i_b].off+(int)workCnt[i_b].val.size()) >= workCnt[i_b+1].off)
		    {
			workCnt[i_b].val.append(workCnt[i_b+1].val,workCnt[i_b].off+(int)workCnt[i_b].val.size()-workCnt[i_b+1].off,string::npos);
			workCnt.erase(workCnt.begin()+i_b+1);
		    }
		}
		else continue;
	    }
	    break;
	}
	if(i_b >= workCnt.size()) workCnt.insert(workCnt.begin()+i_b,SDataRec(reg*2,2));
    }
    //Coils
    else if(dt == "C" || dt == "CI") {
	vector< SDataRec > &workCnt = (dt == "CI") ? acqBlksCoilIn : acqBlksCoil;
	unsigned i_b;
	for(i_b = 0; i_b < workCnt.size(); i_b++) {
	    if(reg < workCnt[i_b].off) {
		if((mMerge || (reg+1) >= workCnt[i_b].off) && (int)(workCnt[i_b].val.size()+workCnt[i_b].off-reg) < blkMaxSz*8)
		{
		    workCnt[i_b].val.insert(0,workCnt[i_b].off-reg,0);
		    workCnt[i_b].off = reg;
		}
		else workCnt.insert(workCnt.begin()+i_b,SDataRec(reg,1));
	    }
	    else if((reg+1) > (workCnt[i_b].off+(int)workCnt[i_b].val.size())) {
		if((mMerge || reg <= (workCnt[i_b].off+(int)workCnt[i_b].val.size())) && (reg+1-workCnt[i_b].off) < blkMaxSz*8)
		{
		    workCnt[i_b].val.append((reg+1)-(workCnt[i_b].off+workCnt[i_b].val.size()),0);
		    // Check for allow mergin to next block
		    if(!mMerge && i_b+1 < workCnt.size() && (workCnt[i_b].off+(int)workCnt[i_b].val.size()) >= workCnt[i_b+1].off)
		    {
			workCnt[i_b].val.append(workCnt[i_b+1].val,workCnt[i_b].off+workCnt[i_b].val.size()-workCnt[i_b+1].off,string::npos);
			workCnt.erase(workCnt.begin()+i_b+1);
		    }
		}
		else continue;
	    }
	    break;
	}
	if(i_b >= workCnt.size()) workCnt.insert(workCnt.begin()+i_b,SDataRec(reg,1));
    }
}

TVariant TMdContr::getVal( const string &addr, MtxString &w_err )
{
    if(tmDelay > 0) {
	if(w_err.getVal().empty()) w_err.setVal(_("10:Connection error or no response."));
	return EVAL_REAL;
    }

    int off = 0;
    string tp = TSYS::strParse(addr, 0, ":", &off);
    string atp_sub = TSYS::strParse(tp, 1, "_");
    bool isInputs = (tp.size() >= 2 && tp[1] == 'I');
    string aids = TSYS::strParse(addr, 0, ":", &off);
    int aid = strtol(aids.c_str(),NULL,0);
    string mode = TSYS::strParse(addr, 0, ":", &off);

    if(tp.empty() || !(mode.empty() || mode == "r" || mode == "rw")) return (int64_t)EVAL_INT;
    if(tp[0] == 'C') return getValC(aid, w_err, isInputs);
    if(tp[0] == 'R') {
	int64_t vl = getValR(aid,w_err,isInputs);
	atp_sub.resize(vmax(2,atp_sub.size()), 0);
	switch(atp_sub[0]) {
	    case 'b':	return char((vl==EVAL_INT)?EVAL_BOOL:((vl>>atoi(atp_sub.c_str()+1))&1));
	    case 'f': {	//Float (4)
		int64_t vl2 = getValR(strtol(TSYS::strParse(aids,1,",").c_str(),NULL,0), w_err, isInputs);
		if(vl == EVAL_INT || vl2 == EVAL_INT) return EVAL_REAL;
		union { uint16_t r[2]; float f; } wl;
		wl.r[0] = vl; wl.r[1] = vl2;
		return wl.f;
	    }
	    case 'd': {	//Double (8)
		int64_t vl2 = getValR(strtol(TSYS::strParse(aids,1,",").c_str(),NULL,0), w_err, isInputs);
		int64_t vl3 = getValR(strtol(TSYS::strParse(aids,2,",").c_str(),NULL,0), w_err, isInputs);
		int64_t vl4 = getValR(strtol(TSYS::strParse(aids,3,",").c_str(),NULL,0), w_err, isInputs);
		if(vl == EVAL_INT || vl2 == EVAL_INT || vl3 == EVAL_INT || vl4 == EVAL_INT) return EVAL_REAL;
		union { uint16_t r[4]; double d; } wl;
		wl.r[0] = vl; wl.r[1] = vl2; wl.r[2] = vl3; wl.r[3] = vl4;
		return wl.d;
	    }
	    case 'i':	//Integer
		switch(atp_sub[1]) {
		    case '2':	return int64_t((vl==EVAL_INT)?EVAL_INT:(int16_t)vl);
		    case '4': {
			int64_t vl2 = getValR(strtol(TSYS::strParse(aids,1,",").c_str(),NULL,0), w_err, isInputs);
			if(vl == EVAL_INT || vl2 == EVAL_INT) return (int64_t)EVAL_INT;
			union { uint16_t r[2]; int32_t i; } wl;
			wl.r[0] = vl; wl.r[1] = vl2;
			return wl.i;
		    }
		    case '8': {
			int64_t vl2 = getValR(strtol(TSYS::strParse(aids,1,",").c_str(),NULL,0), w_err, isInputs);
			int64_t vl3 = getValR(strtol(TSYS::strParse(aids,2,",").c_str(),NULL,0), w_err, isInputs);
			int64_t vl4 = getValR(strtol(TSYS::strParse(aids,3,",").c_str(),NULL,0), w_err, isInputs);
			if(vl == EVAL_INT || vl2 == EVAL_INT || vl3 == EVAL_INT || vl4 == EVAL_INT) return (int64_t)EVAL_INT;
			union { uint16_t r[4]; int64_t i; } wl;
			wl.r[0] = vl; wl.r[1] = vl2; wl.r[2] = vl3; wl.r[3] = vl4;
			return wl.i;
		    }
		}
		break;
	    case 'u':	//Unsigned integer
		switch(atp_sub[1]) {
		    case '2':	return int64_t((vl==EVAL_INT)?EVAL_INT:vl);
		    case '4': {
			int64_t vl2 = getValR(strtol(TSYS::strParse(aids,1,",").c_str(),NULL,0), w_err, isInputs);
			if(vl == EVAL_INT || vl2 == EVAL_INT) return (int64_t)EVAL_INT;
			union { uint16_t r[2]; uint32_t i; } wl;
			wl.r[0] = vl; wl.r[1] = vl2;
			return (int64_t)wl.i;
		    }
		}
		break;
	    case 's': {
		int rSz = strtol(TSYS::strParse(aids,1,",").c_str(), NULL, 0);
		string rez;
		for(int iR = aid; iR < (aid+rSz); iR++) {
		    vl = TSYS::i16_BE(getValR(iR, w_err, isInputs));
		    if(vl == EVAL_INT) return EVAL_STR;
		    rez.append((char*)&vl, 2);
		}
		return rez;
	    }
	    default: return vl;
	}
    }
    return (int64_t)EVAL_INT;
}

int64_t TMdContr::getValR( int addr, MtxString &err, bool in )
{
    int64_t rez = EVAL_INT;
    ResAlloc res(reqRes, false);
    vector<SDataRec>	&workCnt = in ? acqBlksIn : acqBlks;
    for(unsigned i_b = 0; i_b < workCnt.size(); i_b++)
	if((addr*2) >= workCnt[i_b].off && (addr*2+2) <= (workCnt[i_b].off+(int)workCnt[i_b].val.size()))
	{
	    string terr = workCnt[i_b].err.getVal();
	    if(terr.empty())
		rez = (unsigned short)(workCnt[i_b].val[addr*2-workCnt[i_b].off]<<8) |
		      (unsigned char)workCnt[i_b].val[addr*2-workCnt[i_b].off+1];
	    else if(err.getVal().empty()) err.setVal(terr);
	    break;
	}
    return rez;
}

char TMdContr::getValC( int addr, MtxString &err, bool in )
{
    char rez = EVAL_BOOL;
    ResAlloc res(reqRes, false);
    vector<SDataRec>	&workCnt = in ? acqBlksCoilIn : acqBlksCoil;
    for(unsigned i_b = 0; i_b < workCnt.size(); i_b++)
	if(addr >= workCnt[i_b].off && (addr+1) <= (workCnt[i_b].off+(int)workCnt[i_b].val.size()))
	{
	    string terr = workCnt[i_b].err.getVal();
	    if(terr.empty()) rez = workCnt[i_b].val[addr-workCnt[i_b].off];
	    else if(err.getVal().empty()) err.setVal(terr);
	    break;
	}
    return rez;
}

bool TMdContr::setVal( const TVariant &val, const string &addr, MtxString &w_err, bool chkAssync )
{
    if(tmDelay > 0) {
	if(w_err.getVal().empty()) w_err.setVal(_("10:Connection error or no response."));
	return false;
    }

    if(chkAssync && mAsynchWr) { MtxAlloc resAsWr(dataRes(), true); asynchWrs[addr] = val.getS(); return true; }

    int off = 0;
    string tp = TSYS::strParse(addr, 0, ":", &off);
    string atp_sub = TSYS::strParse(tp, 1, "_");
    string aids = TSYS::strParse(addr, 0, ":", &off);
    int aid = strtol(aids.c_str(), NULL, 0);
    string mode = TSYS::strParse(addr, 0, ":", &off);

    bool wrRez = false;
    if(tp.empty() || (tp.size() >= 2 && tp[1] == 'I') || !(mode.empty() || mode == "w" || mode == "rw")) return false;
    if(tp[0] == 'C')	wrRez = setValC(val.getB(), aid, w_err);
    if(tp[0] == 'R') {
	atp_sub.resize(vmax(2,atp_sub.size()), 0);
	switch(atp_sub[0]) {
	    case 'b': {
		int64_t vl = getValR(aid, w_err);
		if(vl != EVAL_INT) wrRez = setValR(val.getB() ? (vl|(1<<atoi(atp_sub.c_str()+1))) : (vl & ~(1<<atoi(atp_sub.c_str()+1))), aid, w_err);
		else if(tmDelay == 0) wrRez = true;	//By no previous data present but need for connect try
		break;
	    }
	    case 'f': {
		union { uint16_t r[2]; float f; } wl;
		wl.f = val.getR();
		map<int,int> regs;
		regs[aid] = wl.r[0];
		regs[strtol(TSYS::strParse(aids,1,",").c_str(),NULL,0)] = wl.r[1];
		wrRez = setValRs(regs, w_err);
		break;
	    }
	    case 'd': {
		union { uint16_t r[4]; double d; } wl;
		wl.d = val.getR();
		map<int, int> regs;
		regs[aid] = wl.r[0];
		regs[strtol(TSYS::strParse(aids,1,",").c_str(),NULL,0)] = wl.r[1];
		regs[strtol(TSYS::strParse(aids,2,",").c_str(),NULL,0)] = wl.r[2];
		regs[strtol(TSYS::strParse(aids,3,",").c_str(),NULL,0)] = wl.r[3];
		wrRez = setValRs(regs, w_err);
		break;
	    }
	    case 'i':
	    case 'u':
		switch(atp_sub[1]) {
		    case '2':	wrRez = setValR(val.getI(), aid, w_err);	break;
		    case '4': {
			union { uint16_t r[2]; uint32_t i; } wl;
			wl.i = val.getI();
			map<int,int> regs;
			regs[aid] = wl.r[0];
			regs[strtol(TSYS::strSepParse(aids,1,',').c_str(),NULL,0)] = wl.r[1];
			wrRez = setValRs(regs, w_err);
			break;
		    }
		    case '8': {
			union { uint16_t r[4]; uint64_t i; } wl;
			wl.i = val.getI();
			map<int,int> regs;
			regs[aid] = wl.r[0];
			regs[strtol(TSYS::strSepParse(aids,1,',').c_str(),NULL,0)] = wl.r[1];
			regs[strtol(TSYS::strSepParse(aids,2,',').c_str(),NULL,0)] = wl.r[2];
			regs[strtol(TSYS::strSepParse(aids,3,',').c_str(),NULL,0)] = wl.r[3];
			wrRez = setValRs(regs, w_err);
			break;
		    }
		}
		break;
	    case 's': {
		string vl = val.getS();
		vl.resize(strtol(TSYS::strSepParse(aids,1,',').c_str(),NULL,0)*2);
		map<int,int> regs;
		for(int iR = aid; iR < (aid+(int)vl.size()/2); iR++)
		    regs[iR] = TSYS::i16_BE(TSYS::getUnalign16(vl.data()+(iR-aid)*2));
		wrRez = setValRs(regs, w_err);
		break;
	    }
	    default: wrRez = setValR(val.getI(), aid, w_err);
	}
    }

    return wrRez;
}

bool TMdContr::setValR( int val, int addr, MtxString &err )
{
    //Encode request PDU (Protocol Data Units)
    string pdu, terr;
    if(!mMltWr) {
	pdu = (char)0x6;		//Function, preset single register
	pdu += (char)(addr>>8);		//Address MSB
	pdu += (char)addr;		//Address LSB
	pdu += (char)(val>>8);		//Data MSB
	pdu += (char)val;		//Data LSB
    }
    else {
	pdu = (char)0x10;		//Function, preset multiple registers
	pdu += (char)(addr>>8);		//Address MSB
	pdu += (char)addr;		//Address LSB
	pdu += (char)0x00;		//Quantity MSB
	pdu += (char)0x01;		//Quantity LSB
	pdu += (char)0x02;		//Byte Count
	pdu += (char)(val>>8);		//Data MSB
	pdu += (char)val;		//Data LSB
    }

    //Request to remote server
    if((terr=modBusReq(pdu)).empty())	numWReg++;
    else {
	if(err.getVal().empty()) err.setVal(terr);
	return false;
    }

    //Set to acquisition block
    ResAlloc res(reqRes, false);
    for(unsigned i_b = 0; i_b < acqBlks.size(); i_b++)
	if((addr*2) >= acqBlks[i_b].off && (addr*2+2) <= (acqBlks[i_b].off+(int)acqBlks[i_b].val.size())) {
	    acqBlks[i_b].val[addr*2-acqBlks[i_b].off]   = (char)(val>>8);
	    acqBlks[i_b].val[addr*2-acqBlks[i_b].off+1] = (char)val;
	    break;
	}

    return true;
}

bool TMdContr::setValRs( const map<int,int> &regs, MtxString &err )
{
    int start = 0, prev = 0;
    string pdu, terr;

    //Write by single register
    if(!mMltWr) {
	for(map<int,int>::const_iterator iR = regs.begin(); iR != regs.end(); iR++)
	    if(!setValR(iR->second, iR->first, err)) return false;
	return true;
    }

    //Write by multiply registers
    for(map<int,int>::const_iterator iR = regs.begin(); true; iR++) {
	if(iR == regs.end() || (pdu.length() && (((iR->first-prev) > 1) || (prev-start) > 122)))
	{
	    if(pdu.empty()) break;
	    // Finish and send request
	    pdu[3] = (char)0x00;		//Quantity MSB
	    pdu[4] = (char)(prev-start+1);	//Quantity LSB
	    pdu[5] = (char)((prev-start+1)*2);	//Byte Count
	    // Request to remote server
	    if((terr=modBusReq(pdu)).empty())	numWReg += (prev-start+1);
	    else {
		if(err.getVal().empty()) err.setVal(terr);
		return false;
	    }

	    pdu = "";
	    if(iR == regs.end()) break;
	}

	//Start request prepare
	if(pdu.empty()) {
	    pdu = (char)0x10;			//Function, preset multiple registers
	    pdu += (char)(iR->first>>8);	//Address MSB
	    pdu += (char)iR->first;		//Address LSB
	    pdu += (char)0x00;			//Quantity MSB
	    pdu += (char)0x01;			//Quantity LSB
	    pdu += (char)0x02;			//Byte Count
	    start = iR->first;
	}
	pdu += (char)(iR->second>>8);		//Data MSB
	pdu += (char)iR->second;		//Data LSB
	prev = iR->first;

	//Set to acquisition block
	ResAlloc res(reqRes, false);
	for(unsigned i_b = 0; i_b < acqBlks.size(); i_b++)
	    if((iR->first*2) >= acqBlks[i_b].off && (iR->first*2+2) <= (acqBlks[i_b].off+(int)acqBlks[i_b].val.size()))
	    {
		acqBlks[i_b].val[iR->first*2-acqBlks[i_b].off]   = (char)(iR->second>>8);
		acqBlks[i_b].val[iR->first*2-acqBlks[i_b].off+1] = (char)iR->second;
		break;
	    }
    }

    return true;
}

bool TMdContr::setValC( char val, int addr, MtxString &err )
{
    //Encode request PDU (Protocol Data Units)
    string pdu, terr;
    if(!mMltWr) {
	pdu = (char)0x5;		//Function, preset single coil
	pdu += (char)(addr>>8);		//Address MSB
	pdu += (char)addr;		//Address LSB
	pdu += (char)(val?0xFF:0x00);	//Data MSB
	pdu += (char)0x00;		//Data LSB
    }
    else {
	pdu = (char)0xF;		//Function, preset multiple coils
	pdu += (char)(addr>>8);		//Address MSB
	pdu += (char)addr;		//Address LSB
	pdu += (char)0x00;		//Quantity MSB
	pdu += (char)0x01;		//Quantity LSB
	pdu += (char)0x01;		//Byte Count
	pdu += (char)(val?0x01:0x00);	//Data MSB
    }
    //Request to remote server
    if((terr=modBusReq(pdu)).empty())	numWCoil++;
    else {
	if(err.getVal().empty()) err.setVal(terr);
	return false;
    }
    //Set to acquisition block
    ResAlloc res(reqRes, false);
    for(unsigned i_b = 0; i_b < acqBlksCoil.size(); i_b++)
	if(addr >= acqBlksCoil[i_b].off && (addr+1) <= (acqBlksCoil[i_b].off+(int)acqBlksCoil[i_b].val.size())) {
	    acqBlksCoil[i_b].val[addr-acqBlksCoil[i_b].off] = val;
	    break;
	}

    return true;
}

string TMdContr::modBusReq( string &pdu )
{
    AutoHD<TTransportOut> tr = SYS->transport().at().at(TSYS::strParse(addr(),0,".")).at().outAt(TSYS::strParse(addr(),1,"."));

    XMLNode req(mPrt);
    req.setAttr("id", id())->
	setAttr("reqTm", i2s(reqTm))->
	setAttr("node", i2s(mNode))->
	setAttr("reqTry", i2s(connTry))->
	setAttr("debugCat", (messLev()==TMess::Debug) ? nodePath() : string(""))->
	setText(pdu);

    tr.at().messProtIO(req, "ModBus");

    if(!req.attr("err").empty()) {
	if(s2i(req.attr("err")) == 14) numErrCon++;
	else numErrResp++;
	if(messLev() >= TMess::Error) mess_err(nodePath().c_str(), "%s", req.attr("err").c_str());
	return req.attr("err");
    }
    pdu = req.text();

    return "";
}

void *TMdContr::Task( void *icntr )
{
    string pdu;
    TMdContr &cntr = *(TMdContr *)icntr;

    cntr.endrunReq = false;
    cntr.prcSt = true;

    bool isStart = true;
    bool isStop  = false;
    int64_t t_cnt = 0, t_prev = TSYS::curTime();

    try {
	while(true) {
	    if(cntr.tmDelay > 0) {
		//Get data from blocks to parameters or calc for logical type parameters
		MtxAlloc prmRes(cntr.enRes, true);
		for(unsigned iP = 0; iP < cntr.pHd.size(); iP++)
		    cntr.pHd[iP].at().upVal(isStart, isStop, cntr.period()?1:-1);
		prmRes.unlock();

		cntr.tmDelay = vmax(0, cntr.tmDelay-1);

		if(isStop) break;

		TSYS::sysSleep(1);

		if(cntr.endrunReq) isStop = true;
		isStart = false;
		continue;
	    }

	    cntr.callSt = true;
	    if(!cntr.period())	t_cnt = TSYS::curTime();

	    //Write asynchronous writings queue
	    MtxAlloc resAsWr(cntr.dataRes(), true);
	    map<string,string> aWrs = cntr.asynchWrs;
	    cntr.asynchWrs.clear();
	    resAsWr.unlock();
	    MtxString asWrErr(cntr.dataRes());
	    for(map<string,string>::iterator iw = aWrs.begin(); !isStart && !isStop && iw != aWrs.end(); ++iw) {
		if(asWrErr.size() && cntr.asynchWrs.find(iw->first) == cntr.asynchWrs.end()) cntr.asynchWrs[iw->first] = iw->second;
		if(!asWrErr.size() && !cntr.setVal(iw->second,iw->first,asWrErr)) { cntr.setCntrDelay(asWrErr); resAsWr.lock(); }
	    }
	    resAsWr.unlock();

	    ResAlloc res(cntr.reqRes, false);

	    //Get coils
	    for(unsigned i_b = 0; !isStart && !isStop && i_b < cntr.acqBlksCoil.size(); i_b++) {
		if(cntr.endrunReq) break;
		if(cntr.redntUse()) { cntr.acqBlksCoil[i_b].err.setVal(_("4:Server failure.")); continue; }
		// Encode request PDU (Protocol Data Units)
		pdu = (char)0x01;					//Function, read multiple coils
		pdu += (char)(cntr.acqBlksCoil[i_b].off>>8);		//Address MSB
		pdu += (char)cntr.acqBlksCoil[i_b].off;			//Address LSB
		pdu += (char)(cntr.acqBlksCoil[i_b].val.size()>>8);	//Number of coils MSB
		pdu += (char)cntr.acqBlksCoil[i_b].val.size();		//Number of coils LSB
		// Request to remote server
		cntr.acqBlksCoil[i_b].err.setVal(cntr.modBusReq(pdu));
		if(cntr.acqBlksCoil[i_b].err.getVal().empty()) {
		    if((cntr.acqBlksCoil[i_b].val.size()/8+((cntr.acqBlksCoil[i_b].val.size()%8)?1:0)) != (pdu.size()-2))
			cntr.acqBlksCoil[i_b].err.setVal(_("15:Response PDU size error."));
		    else {
			for(unsigned i_c = 0; i_c < cntr.acqBlksCoil[i_b].val.size(); i_c++)
			    cntr.acqBlksCoil[i_b].val[i_c] = (bool)((pdu[2+i_c/8]>>(i_c%8))&0x01);
			cntr.numRCoil += cntr.acqBlksCoil[i_b].val.size();
		    }
		}
		else if(s2i(cntr.acqBlksCoil[i_b].err.getVal()) == 14) {
		    cntr.setCntrDelay(cntr.acqBlksCoil[i_b].err.getVal());
		    break;
		}
	    }
	    if(cntr.tmDelay > 0) continue;
	    //Get input's coils
	    for(unsigned i_b = 0; !isStart && !isStop && i_b < cntr.acqBlksCoilIn.size(); i_b++) {
		if(cntr.endrunReq) break;
		if(cntr.redntUse()) { cntr.acqBlksCoilIn[i_b].err.setVal(_("4:Server failure.")); continue; }
		// Encode request PDU (Protocol Data Units)
		pdu = (char)0x02;					//Function, read multiple input's coils
		pdu += (char)(cntr.acqBlksCoilIn[i_b].off>>8);		//Address MSB
		pdu += (char)cntr.acqBlksCoilIn[i_b].off;		//Address LSB
		pdu += (char)(cntr.acqBlksCoilIn[i_b].val.size()>>8);	//Number of coils MSB
		pdu += (char)cntr.acqBlksCoilIn[i_b].val.size();	//Number of coils LSB
		// Request to remote server
		cntr.acqBlksCoilIn[i_b].err.setVal(cntr.modBusReq(pdu));
		if(cntr.acqBlksCoilIn[i_b].err.getVal().empty()) {
		    if((cntr.acqBlksCoilIn[i_b].val.size()/8+((cntr.acqBlksCoilIn[i_b].val.size()%8)?1:0)) != (pdu.size()-2))
			cntr.acqBlksCoilIn[i_b].err.setVal(_("15:Response PDU size error."));
		    else {
			for(unsigned i_c = 0; i_c < cntr.acqBlksCoilIn[i_b].val.size(); i_c++)
			    cntr.acqBlksCoilIn[i_b].val[i_c] = (bool)((pdu[2+i_c/8]>>(i_c%8))&0x01);
			cntr.numRCoilIn += cntr.acqBlksCoilIn[i_b].val.size();
		    }
		}
		else if(s2i(cntr.acqBlksCoilIn[i_b].err.getVal()) == 14) {
		    cntr.setCntrDelay(cntr.acqBlksCoilIn[i_b].err.getVal());
		    break;
		}
	    }
	    if(cntr.tmDelay > 0) continue;
	    //Get registers
	    for(unsigned i_b = 0; !isStart && !isStop && i_b < cntr.acqBlks.size(); i_b++) {
		if(cntr.endrunReq) break;
		if(cntr.redntUse()) { cntr.acqBlks[i_b].err.setVal(_("4:Server failure.")); continue; }
		// Encode request PDU (Protocol Data Units)
		pdu = (char)0x03;				//Function, read multiple registers
		pdu += (char)((cntr.acqBlks[i_b].off/2)>>8);	//Address MSB
		pdu += (char)(cntr.acqBlks[i_b].off/2);		//Address LSB
		pdu += (char)((cntr.acqBlks[i_b].val.size()/2)>>8);	//Number of registers MSB
		pdu += (char)(cntr.acqBlks[i_b].val.size()/2);	//Number of registers LSB
		// Request to remote server
		cntr.acqBlks[i_b].err.setVal(cntr.modBusReq(pdu));
		if(cntr.acqBlks[i_b].err.getVal().empty()) {
		    if(cntr.acqBlks[i_b].val.size() != (pdu.size()-2))
			cntr.acqBlks[i_b].err.setVal(_("15:Response PDU size error."));
		    else {
			cntr.acqBlks[i_b].val.replace(0, cntr.acqBlks[i_b].val.size(), pdu.data()+2, cntr.acqBlks[i_b].val.size());
			cntr.numRReg += cntr.acqBlks[i_b].val.size()/2;
		    }
		}
		else if(s2i(cntr.acqBlks[i_b].err.getVal()) == 14) {
		    cntr.setCntrDelay(cntr.acqBlks[i_b].err.getVal());
		    break;
		}
	    }
	    if(cntr.tmDelay > 0)	continue;
	    //Get input registers
	    for(unsigned i_b = 0; !isStart && !isStop && i_b < cntr.acqBlksIn.size(); i_b++) {
		if(cntr.endrunReq) break;
		if(cntr.redntUse()) { cntr.acqBlksIn[i_b].err.setVal(_("4:Server failure.")); continue; }
		// Encode request PDU (Protocol Data Units)
		pdu = (char)0x04;					//Function, read multiple input registers
		pdu += (char)((cntr.acqBlksIn[i_b].off/2)>>8);		//Address MSB
		pdu += (char)(cntr.acqBlksIn[i_b].off/2);		//Address LSB
		pdu += (char)((cntr.acqBlksIn[i_b].val.size()/2)>>8);	//Number of registers MSB
		pdu += (char)(cntr.acqBlksIn[i_b].val.size()/2);	//Number of registers LSB
		// Request to remote server
		cntr.acqBlksIn[i_b].err.setVal( cntr.modBusReq(pdu));
		if(cntr.acqBlksIn[i_b].err.getVal().empty()) {
		    if(cntr.acqBlksIn[i_b].val.size() != (pdu.size()-2))
			cntr.acqBlksIn[i_b].err.setVal(_("15:Response PDU size error."));
		    else {
			cntr.acqBlksIn[i_b].val.replace(0, cntr.acqBlksIn[i_b].val.size(), pdu.data()+2, cntr.acqBlksIn[i_b].val.size());
			cntr.numRRegIn += cntr.acqBlksIn[i_b].val.size()/2;
		    }
		}
		else if(s2i(cntr.acqBlksIn[i_b].err.getVal()) == 14) {
		    cntr.setCntrDelay(cntr.acqBlksIn[i_b].err.getVal());
		    break;
		}
	    }
	    if(cntr.tmDelay > 0)	continue;
	    res.release();

	    //Get data from blocks to parameters or calc for logical type parameters
	    MtxAlloc prmRes(cntr.enRes, true);
	    for(unsigned iP = 0; iP < cntr.pHd.size(); iP++)
		cntr.pHd[iP].at().upVal(isStart, isStop, cntr.period()?(1e9/(float)cntr.period()):(-1e-6*(t_cnt-t_prev)));
	    isStart = false;
	    prmRes.unlock();

	    //Generic acquisition alarm generate
	    if(cntr.tmDelay <= 0) {
		if(cntr.alSt != 0) {
		    cntr.alSt = 0;
		    cntr.alarmSet(TSYS::strMess(_("DAQ.%s.%s: connect to data source: %s."),cntr.owner().modId().c_str(),cntr.id().c_str(),_("OK")),
			TMess::Info);
		}
		cntr.tmDelay--;
	    }

	    //Calc acquisition process time
	    t_prev = t_cnt;
	    cntr.callSt = false;

	    if(isStop) break;

	    TSYS::taskSleep(cntr.period(), cntr.period() ? "" : cntr.cron());

	    if(cntr.endrunReq) isStop = true;
	}
    } catch(TError &err) { mess_err(err.cat.c_str(), err.mess.c_str()); }

    cntr.prcSt = false;

    return NULL;
}

void TMdContr::setCntrDelay( const string &err )
{
    if(alSt <= 0) {
	alSt = 1;
	alarmSet(TSYS::strMess(_("DAQ.%s.%s: connect to data source: %s."),owner().modId().c_str(),id().c_str(),
								TRegExp(":","g").replace(err,"=").c_str()));
    }
    tmDelay = restTm;
}

TVariant TMdContr::objFuncCall( const string &iid, vector<TVariant> &prms, const string &user )
{
    // string messIO(string pdu) - sending the PDU <pdu> through the controller transpot by ModBus protocol.
    //  pdu - PDU request/respond
    if(iid == "messIO" && prms.size() >= 1 && prms[0].type() == TVariant::String) {
	string req = prms[0].getS();
	string rez = modBusReq(req);
	prms[0].setS(req); prms[0].setModify();
	return rez;
    }
    return TController::objFuncCall(iid, prms, user);
}

void TMdContr::cntrCmdProc( XMLNode *opt )
{
    //Get page info
    if(opt->name() == "info") {
	TController::cntrCmdProc(opt);
	ctrMkNode("fld",opt,-1,"/cntr/cfg/PROT",EVAL_STR,startStat()?R_R_R_:RWRWR_,"root",SDAQ_ID);
	ctrMkNode("fld",opt,-1,"/cntr/cfg/ADDR",EVAL_STR,startStat()?R_R_R_:RWRWR_,"root",SDAQ_ID,
	    3,"tp","str","dest","select","select","/cntr/cfg/trLst");
	ctrMkNode("fld",opt,-1,"/cntr/cfg/NODE",EVAL_STR,startStat()?R_R_R_:RWRWR_,"root",SDAQ_ID);
	ctrMkNode("fld",opt,-1,"/cntr/cfg/MAX_BLKSZ",EVAL_STR,startStat()?R_R_R_:RWRWR_,"root",SDAQ_ID);
	ctrMkNode("fld",opt,-1,"/cntr/cfg/SCHEDULE",EVAL_STR,/*startStat()?R_R_R_:*/RWRWR_,"root",SDAQ_ID,4,
	    "tp","str","dest","sel_ed","sel_list",TMess::labSecCRONsel(),"help",TMess::labSecCRON());
	ctrMkNode("fld",opt,-1,"/cntr/cfg/PRIOR",EVAL_STR,startStat()?R_R_R_:RWRWR_,"root",SDAQ_ID,1,"help",TMess::labTaskPrior());
	ctrMkNode("fld",opt,-1,"/cntr/cfg/FRAG_MERGE",cfg("FRAG_MERGE").fld().descr(),startStat()?R_R_R_:RWRWR_,"root",SDAQ_ID,1,
	    "help",_("Merge not adjacent fragments of registers to single block for request.\n"
		    "Attention! Some devices don't support accompany request wrong registers into single block."));
	ctrMkNode("fld",opt,-1,"/cntr/cfg/TM_REQ",EVAL_STR,RWRWR_,"root",SDAQ_ID,1,
	    "help",_("Individual connection timeout for device requested by the task.\n"
		    "For zero value used generic connection timeout from used output transport."));
	return;
    }

    //Process command to page
    string a_path = opt->attr("path");
    if(a_path == "/cntr/cfg/trLst" && ctrChkNode(opt)) {
	vector<string> sls;
	SYS->transport().at().outTrList(sls);
	for(unsigned i_s = 0; i_s < sls.size(); i_s++)
	    opt->childAdd("el")->setText(sls[i_s]);
    }
    else TController::cntrCmdProc(opt);
}

TMdContr::SDataRec::SDataRec( int ioff, int v_rez ) : off(ioff), err(mod->dataRes())
{
    val.assign(v_rez, 0);
    err.setVal(_("11:Value not gathered."));
}

//******************************************************
//* TMdPrm                                             *
//******************************************************
TMdPrm::TMdPrm( string name, TTypeParam *tp_prm ) : TParamContr(name, tp_prm), pEl("w_attr"), acqErr(dataRes()), lCtx(NULL)
{
    acqErr.setVal("");
    if(isLogic()) lCtx = new TLogCtx(name+"_ModBusPrm");
}

TMdPrm::~TMdPrm( )
{
    nodeDelAll();
    if(lCtx) delete lCtx;
}

void TMdPrm::postEnable( int flag )
{
    TParamContr::postEnable(flag);
    if(!vlElemPresent(&pEl))	vlElemAtt(&pEl);
}

void TMdPrm::postDisable( int flag )
{
    TParamContr::postDisable(flag);

    if(flag && isLogic()) {
	string io_bd = owner().DB()+"."+type().DB(&owner())+"_io";
	TConfig cfg(&mod->prmIOE());
	cfg.cfg("PRM_ID").setS(ownerPath(true), true);
	SYS->db().at().dataDel(io_bd,owner().owner().nodePath()+type().DB(&owner())+"_io",cfg);
    }
}

TCntrNode &TMdPrm::operator=( const TCntrNode &node )
{
    TParamContr::operator=(node);

    const TMdPrm *src_n = dynamic_cast<const TMdPrm*>(&node);
    if(!src_n || !src_n->enableStat() || !enableStat() || !isLogic() || !lCtx) return *this;

    //IO values copy
    for(int iIO = 0; iIO < src_n->lCtx->ioSize(); iIO++)
	if(src_n->lCtx->ioFlg(iIO)&TPrmTempl::CfgLink)
	    lCtx->lnk(lCtx->lnkId(iIO)).addr = src_n->lCtx->lnk(src_n->lCtx->lnkId(iIO)).addr;
    else lCtx->setS(iIO, src_n->lCtx->getS(iIO));

    return *this;
}

void TMdPrm::setType( const string &tpId )
{
    if(lCtx) { delete lCtx; lCtx = NULL; }

    TParamContr::setType(tpId);

    if(isLogic()) lCtx = new TLogCtx(name()+"_ModBusPrm");
}

TMdContr &TMdPrm::owner( ) const	{ return (TMdContr&)TParamContr::owner(); }

bool TMdPrm::isStd( ) const		{ return type().name == "std"; }

bool TMdPrm::isLogic( ) const		{ return type().name == "logic"; }

void TMdPrm::enable( )
{
    if(enableStat() && !owner().isReload) return;

    TParamContr::enable();

    map<string, bool> als;

    //Parse ModBus attributes and convert to string list for standard type parameter
    if(isStd()) {
	string ai, sel, atp, atp_m, atp_sub, aid, anm, awr;
	string m_attrLs = cfg("ATTR_LS").getS();
	for(int ioff = 0; (sel=TSYS::strSepParse(m_attrLs,0,'\n',&ioff)).size(); ) {
	    if(sel[0] == '#') continue;
	    atp = TSYS::strSepParse(sel,0,':');
	    if(atp.empty()) atp = "R";
	    atp_m = TSYS::strSepParse(atp,0,'_');
	    atp_sub = TSYS::strSepParse(atp,1,'_');
	    ai  = TSYS::strSepParse(sel,1,':');
	    awr = TSYS::strSepParse(sel,2,':');
	    aid = TSYS::strSepParse(sel,3,':');
	    if(aid.empty()) aid = ai;
	    anm = TSYS::strSepParse(sel,4,':');
	    if(anm.empty()) anm = aid;

	    if((vlPresent(aid) && !pEl.fldPresent(aid)) || als.find(aid) != als.end())	continue;

	    TFld::Type tp = TFld::Integer;
	    if(atp[0] == 'C' || (atp_sub.size() && atp_sub[0] == 'b')) tp = TFld::Boolean;
	    else if(atp_sub == "f" || atp_sub == "d") tp = TFld::Real;
	    else if(atp_sub == "s") tp = TFld::String;

	    if(!pEl.fldPresent(aid) || pEl.fldAt(pEl.fldId(aid)).type() != tp) {
		if(pEl.fldPresent(aid)) pEl.fldDel(pEl.fldId(aid));
		pEl.fldAdd(new TFld(aid.c_str(),"",tp,TFld::NoFlag));
	    }
	    int el_id = pEl.fldId(aid);

	    unsigned flg = (awr=="rw") ? TVal::DirWrite|TVal::DirRead :
			   ((awr=="w") ? TVal::DirWrite :
					 TFld::NoWrite|TVal::DirRead);
	    if(atp.size() >= 2 && atp[1] == 'I') flg = (flg & (~TVal::DirWrite)) | TFld::NoWrite;
	    pEl.fldAt(el_id).setFlg(flg);
	    pEl.fldAt(el_id).setDescr(anm);

	    if(flg&(TVal::DirRead|TVal::DirWrite)) {
		int reg = strtol(ai.c_str(), NULL, 0);
		if(flg&TVal::DirRead) owner().regVal(reg, atp_m);
		if(atp[0] == 'R') {
		    if(atp_sub == "i4" || atp_sub == "u4" || atp_sub == "f") {
			int reg2 = TSYS::strParse(ai,1,",").empty() ? (reg+1) : strtol(TSYS::strParse(ai,1,",").c_str(),NULL,0);
			if(flg&TVal::DirRead) owner().regVal(reg2, atp_m);
			ai = TSYS::strMess("%d,%d", reg, reg2);
		    }
		    else if(atp_sub == "i8" || atp_sub == "d") {
			int reg2 = TSYS::strParse(ai,1,",").empty() ? (reg+1) : strtol(TSYS::strParse(ai,1,",").c_str(),NULL,0);
			int reg3 = TSYS::strParse(ai,2,",").empty() ? (reg2+1) : strtol(TSYS::strParse(ai,2,",").c_str(),NULL,0);
			int reg4 = TSYS::strParse(ai,3,",").empty() ? (reg3+1) : strtol(TSYS::strParse(ai,3,",").c_str(),NULL,0);
			if(flg&TVal::DirRead) { owner().regVal(reg2, atp_m); owner().regVal(reg3, atp_m); owner().regVal(reg4, atp_m); }
			ai = TSYS::strMess("%d,%d,%d,%d", reg, reg2, reg3, reg4);
		    }
		    else if(atp_sub == "s") {
			int rN = vmax(0,vmin(100,strtol(TSYS::strParse(ai,1,",").c_str(), NULL, 0)));
			if(rN == 0) rN = 10;
			if(flg&TVal::DirRead) for(int iR = reg; iR < (reg+rN); iR++) owner().regVal(iR, atp_m);
			ai = TSYS::strMess("%d,%d", reg, rN);
		    }
		}
	    }
	    pEl.fldAt(el_id).setReserve(atp+":"+ai);

	    als[aid] = true;
	}
    }
    //Template's function connect for logical type parameter
    else if(isLogic() && lCtx)
	try {
	    bool to_make = false;
	    unsigned fId = 0;
	    if(!lCtx->func()) {
		string m_tmpl = cfg("TMPL").getS();
		lCtx->setFunc(&SYS->daq().at().tmplLibAt(TSYS::strSepParse(m_tmpl,0,'.')).at().
						      at(TSYS::strSepParse(m_tmpl,1,'.')).at().func().at());
		to_make = true;
	    }
	    // Init attrubutes
	    for(int iIO = 0; iIO < lCtx->func()->ioSize(); iIO++) {
		if((lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgLink) && lCtx->lnkId(iIO) < 0)
		    lCtx->plnk.push_back(TLogCtx::SLnk(iIO,lCtx->func()->io(iIO)->def()));
		if((lCtx->func()->io(iIO)->flg()&(TPrmTempl::AttrRead|TPrmTempl::AttrFull))) {
		    unsigned flg = TVal::DirWrite|TVal::DirRead;
		    if(lCtx->func()->io(iIO)->flg()&IO::FullText)		flg |= TFld::FullText;
		    if(lCtx->func()->io(iIO)->flg()&TPrmTempl::AttrRead)	flg |= TFld::NoWrite;
		    TFld::Type tp = TFld::type(lCtx->ioType(iIO));
		    if((fId=pEl.fldId(lCtx->func()->io(iIO)->id(),true)) < pEl.fldSize()) {
			if(pEl.fldAt(fId).type() != tp)
			    try{ pEl.fldDel(fId); }
			    catch(TError &err){ mess_warning(err.cat.c_str(),err.mess.c_str()); }
			else {
			    pEl.fldAt(fId).setFlg(flg);
			    pEl.fldAt(fId).setDescr(lCtx->func()->io(iIO)->name().c_str());
			}
		    }

		    if(!vlPresent(lCtx->func()->io(iIO)->id()))
			pEl.fldAdd(new TFld(lCtx->func()->io(iIO)->id().c_str(),lCtx->func()->io(iIO)->name().c_str(),tp,flg));

		    als[lCtx->func()->io(iIO)->id()] = true;
		}
		if(to_make && (lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgLink)) lCtx->setS(iIO,"0");
	    }

	    // Load IO at enabling
	    if(to_make) loadIO(true);

	    // Init links
	    initLnks();

	    // Init system attributes identifiers
	    lCtx->idFreq  = lCtx->ioId("f_frq");
	    lCtx->idStart = lCtx->ioId("f_start");
	    lCtx->idStop  = lCtx->ioId("f_stop");
	    lCtx->idErr   = lCtx->ioId("f_err");
	    lCtx->idSh    = lCtx->ioId("SHIFR");
	    lCtx->idNm    = lCtx->ioId("NAME");
	    lCtx->idDscr  = lCtx->ioId("DESCR");
	    int id_this    = lCtx->ioId("this");
	    if(id_this >= 0) lCtx->setO(id_this, new TCntrNodeObj(AutoHD<TCntrNode>(this),"root"));

	    // First call
	    if(owner().startStat()) upVal(true, false, 0);

	} catch(TError &err) { disable(); throw; }

    //Check for delete DAQ parameter's attributes
    for(int iP = 0; iP < (int)pEl.fldSize(); iP++)
	if(als.find(pEl.fldAt(iP).name()) == als.end())
	    try{ pEl.fldDel(iP); iP--; }
	    catch(TError &err) { mess_warning(err.cat.c_str(),err.mess.c_str()); }

    owner().prmEn(this, true);	//Put to process
}

void TMdPrm::disable( )
{
    if(!enableStat())  return;

    owner().prmEn(this, false);	//Remove from process
    if(lCtx && owner().startStat()) upVal(false, true, 0);

    TParamContr::disable();

    //Set EVAL to parameter attributes
    vector<string> ls;
    elem().fldList(ls);
    for(unsigned i_el = 0; i_el < ls.size(); i_el++)
	vlAt(ls[i_el]).at().setS(EVAL_STR, 0, true);

    //Template's function disconnect
    if(lCtx) {
	lCtx->setFunc(NULL);
	lCtx->idFreq = lCtx->idStart = lCtx->idStop = lCtx->idErr = lCtx->idSh = lCtx->idNm = lCtx->idDscr = -1;
	lCtx->plnk.clear();
    }
}

void TMdPrm::load_( )
{
    //TParamContr::load_();
    loadIO();
}

void TMdPrm::loadIO( bool force )
{
    if(!enableStat() || !isLogic() || !lCtx) return;
    if(owner().startStat() && !force) { modif(true); return; }	//Load/reload IO context only allow for stoped controlers for prevent throws

    //Load IO and init links
    TConfig cfg(&mod->prmIOE());
    cfg.cfg("PRM_ID").setS(ownerPath(true));
    string io_bd = owner().DB()+"."+type().DB(&owner())+"_io";

    for(int iIO = 0; iIO < lCtx->ioSize(); iIO++) {
	cfg.cfg("ID").setS(lCtx->func()->io(iIO)->id());
	if(!SYS->db().at().dataGet(io_bd,owner().owner().nodePath()+type().DB(&owner())+"_io",cfg,false,true)) continue;
	if(lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgLink) lCtx->lnk(lCtx->lnkId(iIO)).addr = cfg.cfg("VALUE").getS();
	else lCtx->setS(iIO,cfg.cfg("VALUE").getS());
    }
    initLnks();
}

void TMdPrm::save_( )
{
    TParamContr::save_();
    saveIO();
}

void TMdPrm::saveIO( )
{
    //Save IO and init links
    if(!enableStat() || !isLogic() || !lCtx) return;

    TConfig cfg(&mod->prmIOE());
    cfg.cfg("PRM_ID").setS(ownerPath(true));
    string io_bd = owner().DB()+"."+type().DB(&owner())+"_io";
    for(int iIO = 0; iIO < lCtx->func()->ioSize(); iIO++) {
	cfg.cfg("ID").setS(lCtx->func()->io(iIO)->id());
	if(lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgLink) cfg.cfg("VALUE").setS(lCtx->lnk(lCtx->lnkId(iIO)).addr);
	else cfg.cfg("VALUE").setS(lCtx->getS(iIO));
	SYS->db().at().dataSet(io_bd,owner().owner().nodePath()+type().DB(&owner())+"_io",cfg);
    }
}

void TMdPrm::initLnks( )
{
    if(!enableStat() || !isLogic()) return;

    string atp, atp_m, atp_sub, ai, mode;
    int reg, off;

    //Init links
    for(int iL = 0; iL < lCtx->lnkSize(); iL++) {
	lCtx->lnk(iL).real = "";
	off = 0;
	atp = TSYS::strParse(lCtx->lnk(iL).addr, 0, ":", &off);
	if(atp.empty()) continue;
	atp_m = TSYS::strParse(atp, 0, "_");
	atp_sub = TSYS::strParse(atp, 1, "_");
	ai  = TSYS::strParse(lCtx->lnk(iL).addr, 0, ":", &off);
	reg = strtol(ai.c_str(),NULL,0);
	mode  = TSYS::strParse(lCtx->lnk(iL).addr, 0, ":", &off);
	if(mode != "w")	owner().regVal(reg, atp_m);
	if(atp[0] == 'R') {
	    if(atp_sub == "i4" || atp_sub == "u4" || atp_sub == "f") {
		int reg2 = TSYS::strParse(ai,1,",").empty() ? (reg+1) : strtol(TSYS::strParse(ai,1,",").c_str(),NULL,0);
		if(mode != "w") owner().regVal(reg2, atp_m);
		ai = TSYS::strMess("%d,%d", reg, reg2);
	    }
	    else if(atp_sub == "i8" || atp_sub == "d") {
		int reg2 = TSYS::strParse(ai,1,",").empty() ? (reg+1) : strtol(TSYS::strParse(ai,1,",").c_str(),NULL,0);
		int reg3 = TSYS::strParse(ai,2,",").empty() ? (reg2+1) : strtol(TSYS::strParse(ai,2,",").c_str(),NULL,0);
		int reg4 = TSYS::strParse(ai,3,",").empty() ? (reg3+1) : strtol(TSYS::strParse(ai,3,",").c_str(),NULL,0);
		if(mode != "w") { owner().regVal(reg2, atp_m); owner().regVal(reg3, atp_m); owner().regVal(reg4, atp_m); }
		ai = TSYS::strMess("%d,%d,%d,%d", reg, reg2, reg3, reg4);
	    }
	    else if(atp_sub == "s") {
		int rN = vmax(0,vmin(100,strtol(TSYS::strParse(ai,1,",").c_str(), NULL, 0)));
		if(rN == 0) rN = 10;
		if(mode != "w") for(int iR = reg; iR < reg+rN; iR++) owner().regVal(iR, atp_m);
		ai = TSYS::strMess("%d,%d", reg, rN);
	    }
	}
	lCtx->lnk(iL).real = atp+":"+ai+":"+mode;
    }
}

void TMdPrm::upVal( bool first, bool last, double frq )
{
    MtxString w_err(dataRes());
    AutoHD<TVal> pVal;
    vector<string> ls;

    if(isStd()) {
	elem().fldList(ls);
	for(unsigned iEl = 0; iEl < ls.size(); iEl++) {
	    pVal = vlAt(ls[iEl]);
	    if(!(pVal.at().fld().flg()&TVal::DirRead) || (pVal.at().fld().flg()&TVal::Dynamic)) continue;
	    pVal.at().set(owner().getVal(pVal.at().fld().reserve(),w_err),0,true);
	}
    }
    else if(isLogic())
	try {
	    //Set fixed system attributes
	    if(lCtx->idFreq >= 0)	lCtx->setR(lCtx->idFreq, frq);
	    if(lCtx->idStart >= 0)	lCtx->setB(lCtx->idStart, first);
	    if(lCtx->idStop >= 0)	lCtx->setB(lCtx->idStop, last);
	    if(lCtx->idSh >= 0)		lCtx->setS(lCtx->idSh, id());
	    if(lCtx->idNm >= 0)		lCtx->setS(lCtx->idNm, name());
	    if(lCtx->idDscr >= 0)	lCtx->setS(lCtx->idDscr, descr());

	    //Get input links
	    for(int iL = 0; iL < lCtx->lnkSize(); iL++)
		if(TSYS::strParse(lCtx->lnk(iL).real,2,":") != "w")	//No read try for only writible
		    lCtx->set(lCtx->lnk(iL).ioId, owner().getVal(lCtx->lnk(iL).real,w_err));

	    //Calc template
	    lCtx->setMdfChk(true);
	    lCtx->calc();
	    modif();

	    //Put output links
	    for(int iL = 0; iL < lCtx->lnkSize(); iL++)
		if(lCtx->ioMdf(lCtx->lnk(iL).ioId))
		    if(!owner().setVal(lCtx->get(lCtx->lnk(iL).ioId), lCtx->lnk(iL).real, w_err))
			lCtx->setS(lCtx->lnk(iL).ioId, EVAL_STR);

	    //Put fixed system attributes
	    if(lCtx->idNm >= 0)  setName(lCtx->getS(lCtx->idNm));
	    if(lCtx->idDscr >= 0)setDescr(lCtx->getS(lCtx->idDscr));

	    //Attribute's values update
	    elem().fldList(ls);
	    for(unsigned iEl = 0; iEl < ls.size(); iEl++) {
		int id_lnk = lCtx->lnkId(ls[iEl]);
		if(id_lnk >= 0 && lCtx->lnk(id_lnk).real.empty()) id_lnk = -1;
		pVal = vlAt(ls[iEl]);
		if(pVal.at().fld().flg()&TVal::Dynamic)	continue;
		if(id_lnk < 0) pVal.at().set(lCtx->get(lCtx->ioId(ls[iEl])), 0, true);
		else pVal.at().set(owner().getVal(lCtx->lnk(id_lnk).real,acqErr), 0, true);
	    }
	} catch(TError &err) {
	    mess_warning(err.cat.c_str(),"%s",err.mess.c_str());
	    mess_warning(nodePath().c_str(),_("Error calculate template."));
	}

    //Alarm set
    acqErr.setVal(w_err.getVal());
}

TVariant TMdPrm::objFuncCall( const string &iid, vector<TVariant> &prms, const string &user )
{
    //bool attrAdd( string id, string name, string tp = "real", string selValsNms = "" ) - attribute <id> and <name> for type <tp> add.
    //  id, name - new attribute id and name;
    //  tp - attribute type [boolean | integer | real | string | text | object] + selection mode [sel | seled] + read only [ro];
    //  selValsNms - two lines with values in first and it's names in first (separated by ";").
    if(iid == "attrAdd" && prms.size() >= 1) {
	if(!enableStat() || !isLogic())	return false;
	TFld::Type tp = TFld::Real;
	string stp, stp_ = (prms.size() >= 3) ? prms[2].getS() : "real";
	stp.resize(stp_.length());
	std::transform(stp_.begin(), stp_.end(), stp.begin(), ::tolower);
	if(stp.find("boolean") != string::npos)		tp = TFld::Boolean;
	else if(stp.find("integer") != string::npos)	tp = TFld::Integer;
	else if(stp.find("real") != string::npos)	tp = TFld::Real;
	else if(stp.find("string") != string::npos ||
		stp.find("text") != string::npos)	tp = TFld::String;
	else if(stp.find("object") != string::npos)	tp = TFld::Object;

	unsigned flg = TVal::Dynamic;
	if(stp.find("sel") != string::npos)	flg |= TFld::Selected;
	if(stp.find("seled") != string::npos)	flg |= TFld::SelEdit;
	if(stp.find("text") != string::npos)	flg |= TFld::FullText;
	if(stp.find("ro") != string::npos)	flg |= TFld::NoWrite;

	string	sVals = (prms.size() >= 4) ? prms[3].getS() : "";
	string	sNms = TSYS::strLine(sVals, 1);
	sVals = TSYS::strLine(sVals, 0);

	MtxAlloc res(elem().resEl(), true);
	unsigned aId = elem().fldId(prms[0].getS(), true);
	if(aId < elem().fldSize()) {
	    if(prms.size() >= 2 && prms[1].getS().size()) elem().fldAt(aId).setDescr(prms[1].getS());
	    elem().fldAt(aId).setFlg(elem().fldAt(aId).flg()^((elem().fldAt(aId).flg()^flg)&(TFld::Selected|TFld::SelEdit|TFld::FullText|TFld::NoWrite)));
	    elem().fldAt(aId).setValues(sVals);
	    elem().fldAt(aId).setSelNames(sNms);
	    elem().fldAt(aId).setLen(SYS->sysTm());
	}
	else if(!vlPresent(prms[0].getS()))
	    elem().fldAdd(new TFld(prms[0].getS().c_str(),prms[(prms.size()>=2)?1:0].getS().c_str(),tp,flg,
				    i2s(SYS->sysTm()).c_str(),"",sVals.c_str(),sNms.c_str()));
	return true;
    }
    //bool attrDel( string id ) - attribute <id> remove.
    if(iid == "attrDel" && prms.size() >= 1) {
	if(!enableStat() || !isLogic())	return false;
	MtxAlloc res(elem().resEl(), true);
	unsigned aId = elem().fldId(prms[0].getS(), true);
	if(aId == elem().fldSize())	return false;
	try { elem().fldDel(aId); } catch(TError&) { return false; }
	return true;
    }

    return TParamContr::objFuncCall(iid, prms, user);
}

void TMdPrm::vlGet( TVal &val )
{
    if(!enableStat() || !owner().startStat()) {
	if(val.name() == "err") {
	    if(!enableStat())			val.setS(_("1:Parameter is disabled."),0,true);
	    else if(!owner().startStat())	val.setS(_("2:Acquisition is stopped."),0,true);
	}
	else val.setS(EVAL_STR, 0, true);
	return;
    }

    if(owner().redntUse()) return;

    if(val.name() == "err") {
	if(acqErr.getVal().size()) val.setS(acqErr.getVal(),0,true);
	else if(lCtx && lCtx->idErr >= 0) val.setS(lCtx->getS(lCtx->idErr),0,true);
	else val.setS("0",0,true);
    }
}

void TMdPrm::vlSet( TVal &vo, const TVariant &vl, const TVariant &pvl )
{
    if(!enableStat() || !owner().startStat())	{ vo.setS(EVAL_STR, 0, true); return; }

    if(vl.isEVal() || vl == pvl) return;

    //Send to active reserve station
    if(owner().redntUse()) {
	XMLNode req("set");
	req.setAttr("path",nodePath(0,true)+"/%2fserv%2fattr")->childAdd("el")->setAttr("id",vo.name())->setText(vl.getS());
	SYS->daq().at().rdStRequest(owner().workId(),req);
	return;
    }

    //Direct write
    bool wrRez = false;
    // Standard type request
    if(isStd())	wrRez = owner().setVal(vl, vo.fld().reserve(), acqErr, true);
    // Logical type request
    else if(isLogic()) {
	int id_lnk = lCtx->lnkId(vo.name());
	if(id_lnk >= 0 && lCtx->lnk(id_lnk).real.empty()) id_lnk = -1;
	if(id_lnk < 0) { lCtx->set(lCtx->ioId(vo.name()), vl); wrRez = true; }
	else wrRez = owner().setVal(vl, lCtx->lnk(id_lnk).real, acqErr, true);
    }
    if(!wrRez) vo.setS(EVAL_STR, 0, true);
}

void TMdPrm::vlArchMake( TVal &val )
{
    TParamContr::vlArchMake(val);

    if(val.arch().freeStat()) return;
    val.arch().at().setSrcMode(TVArchive::PassiveAttr);
    val.arch().at().setPeriod(owner().period() ? owner().period()/1000 : 1000000);
    val.arch().at().setHardGrid(true);
    val.arch().at().setHighResTm(true);
}

void TMdPrm::cntrCmdProc( XMLNode *opt )
{
    //Get page info
    if(opt->name() == "info") {
	TParamContr::cntrCmdProc(opt);
	if(isStd())
	    ctrMkNode("fld",opt,-1,"/prm/cfg/ATTR_LS",EVAL_STR,(owner().startStat()&&enableStat())?R_R_R_:RWRWR_,"root",SDAQ_ID,3,
		"rows","8","SnthHgl","1",
		"help",_("Attributes configuration list. List must be written by lines in format: \"{dt}:{numb}:{rw}:{id}:{name}\".\n"
		    "Where:\n"
		    "  dt - ModBus data type (R-register[3,6(16)], C-coil[1,5(15)], RI-input register[4], CI-input coil[2]);\n"
		    "       R and RI can be expanded by suffixes:\n"
		    "         i2-Int16, i4-Int32, i8-Int64, u2-UInt16, u4-UInt32, f-Float, d-Double, b5-Bit5, s-String;\n"
		    "       Start from symbol '#' for comment line;\n"
		    "  numb - ModBus device's data address (dec, hex or octal) [0...65535];\n"
		    "  rw - read/write mode (r-read; w-write; rw-readwrite);\n"
		    "  id - created attribute identifier;\n"
		    "  name - created attribute name.\n"
		    "Examples:\n"
		    "  \"R:0x300:rw:var:Variable\" - register access;\n"
		    "  \"C:100:rw:var1:Variable 1\" - coin access;\n"
		    "  \"R_f:200:r:float:Float\" - get float from registers 200 and 201;\n"
		    "  \"R_i4:400,300:r:int32:Int32\" - get int32 from registers 400 and 300;\n"
		    "  \"R_b10:25:r:rBit:Reg bit\" - get bit 10 from register 25;\n"
		    "  \"R_s:15,20:r:str:Reg blk\" - get string, registers block, from register 15 and size 20."));
	if(isLogic()) {
	    ctrMkNode("fld",opt,-1,"/prm/cfg/TMPL",EVAL_STR,RWRW__,"root",SDAQ_ID,3,"tp","str","dest","select","select","/prm/tmplList");
	    if(enableStat() && ctrMkNode("area",opt,-1,"/cfg",_("Template configuration"))) {
		if(ctrMkNode("area",opt,-1,"/cfg/prm",_("Parameters")))
		for(int iIO = 0; iIO < lCtx->ioSize(); iIO++) {
		    if(!(lCtx->func()->io(iIO)->flg()&(TPrmTempl::CfgLink|TPrmTempl::CfgConst))) continue;
		    // Check select param
		    if(lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgLink)
			ctrMkNode("fld",opt,-1,(string("/cfg/prm/el_")+i2s(iIO)).c_str(),lCtx->func()->io(iIO)->name(),RWRWR_,"root",SDAQ_ID,2,"tp","str",
			    "help",_("ModBus address in format: \"{dt}:{numb}:{rw}\".\n"
				"Where:\n"
				"  dt - ModBus data type (R-register[3,6(16)], C-coil[1,5(15)], RI-input register[4], CI-input coil[2]);\n"
				"       R and RI can be expanded by suffixes:\n"
				"         i2-Int16, i4-Int32, i8-Int64, u2-UInt16, u4-UInt32, f-Float, d-Double, b5-Bit5, s-String;\n"
				"  numb - ModBus device's data address (dec, hex or octal) [0...65535];\n"
				"  rw - read/write mode (r-read; w-write; rw-readwrite).\n"
				"Examples:\n"
				"  \"R:0x300:rw\" - register access;\n"
				"  \"C:100:rw\" - coin access;\n"
				"  \"R_f:200:r\" - get float from registers 200 and 201;\n"
				"  \"R_i4:400,300:r\" - get int32 from registers 400 and 300;\n"
				"  \"R_b10:25:r\" - get bit 10 from register 25;\n"
				"  \"R_s:15,20:r\" - get string, registers block, from register 15 and size 20."));
		    else {
			const char *tip = "str";
			bool fullTxt = false;
			switch(lCtx->ioType(iIO)) {
			    case IO::Integer:	tip = "dec";	break;
			    case IO::Real:	tip = "real";	break;
			    case IO::Boolean:	tip = "bool";	break;
			    case IO::String:
				if(lCtx->func()->io(iIO)->flg()&IO::FullText) fullTxt = true;
				break;
			    case IO::Object:	fullTxt = true;	break;
			}
			XMLNode *wn = ctrMkNode("fld",opt,-1,(string("/cfg/prm/el_")+i2s(iIO)).c_str(),
					lCtx->func()->io(iIO)->name(),RWRWR_,"root",SDAQ_ID,1,"tp",tip);
			if(wn && fullTxt) wn->setAttr("cols","100")->setAttr("rows","4");
		    }
		}
	    }
	}
	return;
    }
    //Process command to page
    string a_path = opt->attr("path");
    if(isStd() && a_path == "/prm/cfg/ATTR_LS" && ctrChkNode(opt,"SnthHgl",RWRWR_,"root",SDAQ_ID,SEC_RD)) {
	opt->childAdd("rule")->setAttr("expr","^#[^\n]*")->setAttr("color","gray")->setAttr("font_italic","1");
	opt->childAdd("rule")->setAttr("expr",":(r|w|rw):")->setAttr("color","red");
	opt->childAdd("rule")->setAttr("expr",":(0[xX][0-9a-fA-F]*|[0-9]*),?(0[xX][0-9a-fA-F]*|[0-9]*),?(0[xX][0-9a-fA-F]*|[0-9]*),?(0[xX][0-9a-fA-F]*|[0-9]*)")->setAttr("color","blue");
	opt->childAdd("rule")->setAttr("expr","^(C|CI|R|RI|RI?_[iubfds]\\d*)")->setAttr("color","darkorange");
	opt->childAdd("rule")->setAttr("expr","\\:")->setAttr("color","blue");
    }
    else if(isLogic() && a_path == "/prm/cfg/TMPL" && ctrChkNode(opt,"set",RWRW__,"root",SDAQ_ID,SEC_WR)) {
	cfg("TMPL").setS(opt->text());
	disable();
	modif();
    }
    else if(isLogic() && enableStat() && a_path.substr(0,12) == "/cfg/prm/el_") {
	if(ctrChkNode(opt,"get",RWRWR_,"root",SDAQ_ID,SEC_RD)) {
	    int iIO = s2i(a_path.substr(12));
	    if(lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgLink) opt->setText(lCtx->lnk(lCtx->lnkId(iIO)).addr);
	    else if(lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgConst) opt->setText(lCtx->getS(iIO));
	}
	if(ctrChkNode(opt,"set",RWRWR_,"root",SDAQ_ID,SEC_WR)) {
	    int iIO = s2i(a_path.substr(12));
	    if(lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgLink) {
		lCtx->lnk(lCtx->lnkId(iIO)).addr = opt->text();
		initLnks();
	    }
	    else if(lCtx->func()->io(iIO)->flg()&TPrmTempl::CfgConst) lCtx->setS(iIO,opt->text());
	    modif();
	}
    }
    else TParamContr::cntrCmdProc(opt);
}

//***************************************************
//* Logical type parameter's context                *
TMdPrm::TLogCtx::TLogCtx( const string &name ) : TValFunc(name),
    idFreq(-1), idStart(-1), idStop(-1), idErr(-1), idSh(-1), idNm(-1), idDscr(-1)
{

}

int TMdPrm::TLogCtx::lnkId( int id )
{
    for(unsigned iL = 0; iL < plnk.size(); iL++)
	if(lnk(iL).ioId == id)
	    return iL;
    return -1;
}

int TMdPrm::TLogCtx::lnkId( const string &id )
{
    for(unsigned iL = 0; iL < plnk.size(); iL++)
	if(func()->io(lnk(iL).ioId)->id() == id)
	    return iL;
    return -1;
}

TMdPrm::TLogCtx::SLnk &TMdPrm::TLogCtx::lnk( int num )
{
    if(num < 0 || num >= (int)plnk.size()) throw TError(mod->nodePath().c_str(),_("Parameter id error."));
    return plnk[num];
}

//*******************************************************
//* Link object of the logical type parameter's context *
TMdPrm::TLogCtx::SLnk::SLnk( ) : addr(mod->dataRes()), real(mod->dataRes())
{

}

TMdPrm::TLogCtx::SLnk::SLnk( int iid, const string &iaddr ) : ioId(iid), addr(mod->dataRes()), real(mod->dataRes())
{
    addr = iaddr;
}
