/***************************************************************************
 *   Copyright (C) 2004 by Roman Savochenko                                *
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

#ifndef TPARAM_H
#define TPARAM_H

#include <string>
#include <vector>

#include "tconfig.h"
#include "tfunction.h"
#include "tvalue.h"

using std::string;
using std::vector;

class TParamContr;
class TParamS;

class TParam : public TValue, public TConfig
{
    class SLnk
    {
        public:
            SLnk(int iid, int imode, const string &iprm_attr = "") :
                io_id(iid), mode(imode), prm_attr(iprm_attr) { }
            int		io_id;
	    int		mode;
            string      prm_attr;
            AutoHD<TParamContr> prm;
	    string      attr;
    };

    /** Public methods: */
    public:
	enum Mode { Clear, DirRefl, Template };
    
	TParam( const string &iid, TElem *cf_el );
	~TParam(  );

	const string &id()	{ return m_id; }
	const string &name() 	{ return m_name; }
	const string &descr()	{ return m_descr; }
	
	void name( const string &inm )	{ m_name = inm; }
	void descr( const string &idsc ){ m_descr = idsc; }
	
	bool toEnable()         { return m_aen; }
        bool enableStat()       { return m_en; }
	
	Mode mode()	{ return m_wmode; }
	void mode( Mode md, const string &prm = "" );
	
        void enable();
        void disable();
	
        void load( );
        void save( );	
	
	void calc();	//Calc template's algoritmes

    	TParamS &owner() { return *(TParamS*)nodePrev(); }
	
    private:	    
	string nodeName(){ return m_id; }
	void postEnable( );
	void preDisable(int flag);
	void postDisable(int flag);
	//================== Controll functions ========================
        void cntrCmd_( const string &a_path, XMLNode *opt, TCntrNode::Command cmd );		

	void vlGet( TVal &val );
        void vlSet( TVal &val );
	
	//Template link operations
	int lnkSize();
	int lnkId( int id );
	SLnk &lnk( int num );

	void loadIO();
	void saveIO();
	void initTmplLnks();
	
    private:
	string 	&m_id, &m_name, &m_descr, &m_prm, m_wprm;
	bool    &m_aen,         //Auto-enable
		m_en;           //Enable stat
	int	&m_mode;	//Config parameter mode
	Mode	m_wmode;	//Work parameter mode
		
	TElem 	p_el;		//Work atribute elements
		
	//Parameter template structure
	struct STmpl
        {
	    TValFunc     val;
	    vector<SLnk> lnk;
	};	
	
	union
	{
	    AutoHD<TParamContr> *prm_refl;	//Direct reflection
	    STmpl *tmpl;			//Template
	};
};

#endif // TPARAM_H
