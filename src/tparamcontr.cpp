
//OpenSCADA system file: tparamcontr.cpp
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
 
#include "tbds.h"
#include "tsys.h"
#include "tmess.h"
#include "tdaqs.h"
#include "tcontroller.h"
#include "ttipdaq.h"
#include "ttiparam.h"
#include "tparamcontr.h"

TParamContr::TParamContr( const string &name, TTipParam *tpprm ) : 
    TConfig(tpprm), tipparm(tpprm), m_en(false), m_export(false),
    m_id(cfg("SHIFR").getSd()), m_name(cfg("NAME").getSd()), m_descr(cfg("DESCR").getSd()), m_aen(cfg("EN").getBd())
{
    m_id = m_name = name;
}

TParamContr::~TParamContr( )
{
    nodeDelAll();
}

string TParamContr::name()
{ 
    return (m_name.size())?m_name:m_id;
}

void TParamContr::postEnable()
{
    TValue::postEnable();
    if(!vlCfg())  vlCfg(this);	
    if(!vlElemPresent(&SYS->daq().at().errE())) 
	vlElemAtt(&SYS->daq().at().errE());
}

void TParamContr::preDisable(int flag)
{
    if( flag )
    {
	//Delete archives
	vector<string> a_ls;
	vlList(a_ls);
	for(int i_a = 0; i_a < a_ls.size(); i_a++)
	    if( !vlAt(a_ls[i_a]).at().arch().freeStat() )
	    {
		string arh_id = vlAt(a_ls[i_a]).at().arch().at().id();
		SYS->archive().at().valDel(arh_id,true);
	    }
    }    

    if( enableStat() )	disable();
}
	
void TParamContr::postDisable(int flag)
{
    if( flag )
    {	
	//Delete parameter from DB
	try
	{
	    SYS->db().at().dataDel(owner().genBD()+"."+owner().cfg(type().BD()).getS(),
		    		   owner().owner().nodePath()+owner().cfg(type().BD()).getS(),*this);
	}catch(TError err) { Mess->put(err.cat.c_str(),TMess::Error,"%s",err.mess.c_str()); }
    }
}

void TParamContr::load( )
{
    SYS->db().at().dataGet(owner().genBD()+"."+owner().cfg(type().BD()).getS(),
	    		   owner().owner().nodePath()+owner().cfg(type().BD()).getS(),*this);
}

void TParamContr::save( )
{
    SYS->db().at().dataSet(owner().genBD()+"."+owner().cfg(type().BD()).getS(),
	    		   owner().owner().nodePath()+owner().cfg(type().BD()).getS(),*this);
    
    //Save archives
    vector<string> a_ls;
    vlList(a_ls);
    for(int i_a = 0; i_a < a_ls.size(); i_a++)
        if( !vlAt(a_ls[i_a]).at().arch().freeStat() )
            vlAt(a_ls[i_a]).at().arch().at().save();				    
}

TParamContr & TParamContr::operator=( TParamContr & PrmCntr )
{
    TConfig::operator=(PrmCntr);

    return *this;
}

void TParamContr::enable()
{
    m_en = true;
}

void TParamContr::disable()
{
    m_en = false;
}

void TParamContr::vlGet( TVal &val )
{
    if(val.name() == "err" )
    {
	if( enableStat() ) val.setS("0",0,true);
	else val.setS(Mess->I18N("1:Parameter had disabled."),0,true);
    }
}

void TParamContr::cntrCmdProc( XMLNode *opt )
{
    //Get page info
    if( opt->name() == "info" )
    {
	TValue::cntrCmdProc(opt);
	ctrMkNode("oscada_cntr",opt,-1,"/",Mess->I18Ns("Parameter: ")+name());
	ctrMkNode("area",opt,0,"/prm",Mess->I18N("Parameter"));
	ctrMkNode("area",opt,-1,"/prm/st",Mess->I18N("State"));
	ctrMkNode("fld",opt,-1,"/prm/st/type",Mess->I18N("Type"),0444,"root","root",1,"tp","str");
	if( owner().startStat() ) 
	    ctrMkNode("fld",opt,-1,"/prm/st/en",Mess->I18N("Enable"),0664,"root","root",1,"tp","bool");
	ctrMkNode("area",opt,-1,"/prm/cfg",Mess->I18N("Config"));
	ctrMkNode("comm",opt,-1,"/prm/cfg/load",Mess->I18N("Load"),0440);
	ctrMkNode("comm",opt,-1,"/prm/cfg/save",Mess->I18N("Save"),0440);
	TConfig::cntrCmdMake(opt,"/prm/cfg",0);
        return;
    }
    //Process command to page
    string a_path = opt->attr("path");
    if( a_path == "/prm/st/type" && ctrChkNode(opt) )	opt->text(type().lName());
    else if( a_path == "/prm/st/en" )
    {
	if( ctrChkNode(opt,"get",0664,"root","root",SEQ_RD) )	opt->text(enableStat()?"1":"0");
	if( ctrChkNode(opt,"set",0664,"root","root",SEQ_WR) )
	{
	    if( !owner().startStat() )	throw TError(nodePath().c_str(),"Controller no started!");
	    else atoi(opt->text().c_str())?enable():disable();
	}
    }
    else if( a_path == "/prm/cfg/load" && ctrChkNode(opt,"set",0440) ) 	load();
    else if( a_path == "/prm/cfg/save" && ctrChkNode(opt,"set",0440) ) 	save();    
    else if( a_path.substr(0,8) == "/prm/cfg" ) TConfig::cntrCmdProc(opt,TSYS::pathLev(a_path,2));
    else TValue::cntrCmdProc(opt);
}                                                                                             
