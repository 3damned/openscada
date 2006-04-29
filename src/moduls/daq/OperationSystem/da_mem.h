
//OpenSCADA system module DAQ.OperationSystem file: da_mem.h
/***************************************************************************
 *   Copyright (C) 2005-2006 by Roman Savochenko                           *
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
 
#ifndef DA_MEM_H
#define DA_MEM_H

#include "da.h"

namespace SystemCntr
{

class Mem: public DA
{
    public:
	Mem( );
	~Mem( );
	
        string id()     { return "MEM"; }
        string name()   { return "Memory"; }			

	void init( TMdPrm *prm );
	void deInit( TMdPrm *prm );
	void getVal( TMdPrm *prm );
	void setEVAL( TMdPrm *prm );
	
	void makeActiveDA( TMdContr *a_cntr );
};

} //End namespace 

#endif //DA_MEM_H

