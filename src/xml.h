
//OpenSCADA system file: xml.h
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

#ifndef XML_H
#define XML_H

#include <string>
#include <vector>

#include <expat.h>

using std::string;
using std::vector;
using std::pair;

//*************************************************
//* XMLNode                                       *
//*************************************************
class XMLNode
{
    public:
	//Data
	enum SaveView
	{
	    BrOpenPrev		= 0x01,		//Break preview open tag
	    BrOpenPast		= 0x02,		//Break past open tag
	    BrClosePast		= 0x04,		//Break past close tag
	    BrTextPast		= 0x08,		//Break past text
	    BrPrcInstrPast	= 0x10,		//Break past process instruction
	    BrAllPast		= 0x1E		//Break past all
	};

	//Methods
	XMLNode( const string &name = "" ) : mName(name), mText(""), mParent(NULL)	{  }
	~XMLNode( )				{ clear(); }

	XMLNode &operator=( XMLNode &prm );

	string	name( ) const			{ return mName; }
	XMLNode* setName( const string &s )	{ mName = s; return this; }

	string	text( ) const			{ return mText; }
	XMLNode* setText( const string &s )	{ mText = s; return this; }

	void	attrList( vector<string> & list ) const;
	XMLNode* attrDel( const string &name );
	void	attrClear( );
	string	attr( const string &name ) const;
	XMLNode* setAttr( const string &name, const string &val );

	void	prcInstrList( vector<string> & list ) const;
	void	prcInstrDel( const string &target );
	void	prcInstrClear( );
	string	prcInstr( const string &target ) const;
	XMLNode* setPrcInstr( const string &target, const string &val );

	void	load( const string &vl );
	string	save( unsigned char flgs = 0 );
	XMLNode* clear( );

	int	childSize( ) const		{ return mChildren.size(); }
	void	childAdd( XMLNode *nd );
	XMLNode* childAdd( const string &name = "" );
	int	childIns( unsigned id, XMLNode *nd );
	XMLNode* childIns( unsigned id, const string &name = "" );
	void	childDel( const unsigned id );
	void	childClear( const string &name = "" );
	XMLNode* childGet( const int, bool noex = false ) const;
	XMLNode* childGet( const string &name, const int numb = 0, bool noex = false ) const;
	XMLNode* childGet( const string &attr, const string &name, bool noex = false ) const;

	XMLNode* parent( )			{ return mParent; }
    private:
	//Methods
	string encode( const string &s, bool text = false ) const;

	static void start_element( void *data, const char *el, const char **attr );
	static void end_element( void *data, const char *el );
	static void characters( void *userData, const XML_Char *s, int len );
	static void instrHandler( void *userData, const XML_Char *target, const XML_Char *data );

	//Attributes
	string mName;
	string mText;
	vector< XMLNode* >		mChildren;
	vector< pair<string,string> >	mAttr;
	vector< pair<string,string> >	mPrcInstr;
	XMLNode* mParent;

	//- Parse/load XML attributes -
	vector<XMLNode*> node_stack;
};

#endif  //XML_H
