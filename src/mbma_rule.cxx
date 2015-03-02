/*
  $Id: mbma_mod.cxx 18070 2015-02-18 15:05:05Z sloot $
  $URL: https://ilk.uvt.nl/svn/sources/Frog/trunk/src/mbma_mod.cxx $

  Copyright (c) 2006 - 2015
  Tilburg University

  This file is part of frog.

  frog is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  frog is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
*/

#include <string>
#include <vector>
#include <iostream>
#include "libfolia/document.h"
#include "ucto/unicode.h"
#include "frog/clex.h"
#include "frog/mbma_rule.h"

using namespace std;
using namespace folia;

bool RulePart::isBasic() const {
  return isBasicClass( ResultClass );
}

ostream& operator<<( ostream& os, const RulePart& r ){
  if ( r.ResultClass == CLEX::UNASS &&
       r.inflect.empty() ){
    os << "INVALID! No result node, AND no inflection" << endl;
  }
  else {
    os << r.uchar << " - ";
    for ( size_t i = 0; i < r.RightHand.size(); ++i ){
      os << r.RightHand[i];
      if ( i < r.RightHand.size()-1 ){
	os << "+";
      }
    }
    if ( !r.RightHand.empty() ){
      os << " ==> ";
    }
    os << r.ResultClass << " ";
    if ( !r.inflect.empty() ){
      os << " INFLECTION: " << r.inflect;
    }
  }
  if ( r.fixpos >= 0 )
    os << " affix at pos: " << r.fixpos;
  if ( r.xfixpos >= 0 )
    os << " x-affix at pos: " << r.xfixpos;
  if ( !r.ins.isEmpty() )
    os << " insert='" << r.ins << "'";
  if ( !r.del.isEmpty() )
    os << " delete='" << r.del << "'";
  if ( !r.morpheme.isEmpty() )
    os << " morpheme ='" << r.morpheme << "'";
  return os;
}

ostream& operator<<( ostream& os, const RulePart *r ){
  return os << *r;
}

void RulePart::get_ins_del( const string& edit ){
  if (edit[0]=='D') { // delete operation
    string s = edit.substr(1);
    ins = UTF8ToUnicode( s );
  }
  else if ( edit[0]=='I') {  //insert operation
    string s = edit.substr(1);
    del = UTF8ToUnicode( s );
  }
  else if ( edit[0]=='R') { // replace operation
    string::size_type pos = edit.find( ">" );
    string s = edit.substr( 1, pos-1 );
    ins = UTF8ToUnicode( s );
    s = edit.substr( pos+1 );
    del = UTF8ToUnicode( s );
  }
}

RulePart::RulePart( const string& rs, const UChar kar ):
  ResultClass(CLEX::UNASS),
  uchar(kar),
  fixpos(-1),
  xfixpos(-1),
  participle(false)
{
  //  cerr << "extract RulePart:" << rs << endl;
  string edit;
  string s = rs;
  string::size_type ppos = rs.find("+");
  if ( ppos != string::npos ){
    // some edit info is available
    string::size_type spos = rs.find("/");
    if ( spos != string::npos ){
      // inflection too
      inflect = rs.substr( spos+1 );
      //    cerr << "inflect = " << inflect << endl;
      edit = rs.substr( ppos+1, spos-ppos-1 );
    }
    else {
      edit = rs.substr( ppos+1 );
    }
    //    cerr << "EDIT = " << edit << endl;
    get_ins_del( edit );
    s = rs.substr(0, ppos );
    participle = ( s.find( 'p' ) != string::npos ) &&
      ( del == "ge" || del == "be" );
  }
  string::size_type pos = s.find("_");
  if ( pos != string::npos ){
    ResultClass = CLEX::toCLEX( s[0] );
    // a rewrite RulePart
    if ( pos != 1 ){
      cerr << "Surprise! _ on a strange position:" << pos << " in " << s << endl;
    }
    else {
      string rhs = s.substr( pos+1 );
      //      cerr << "RHS = " << rhs << endl;
      string::size_type spos = rhs.find("/");
      if ( spos != string::npos ){
	// inflection too
	inflect = rhs.substr( spos+1 );
	// cerr << "inflect = " << inflect << endl;
	rhs = rhs.substr( 0, spos );
      }
      //      cerr << "RHS = " << rhs << endl;
      RightHand.resize( rhs.size() );
      for ( size_t i = 0; i < rhs.size(); ++i ){
	CLEX::Type tag = CLEX::toCLEX( rhs[i] );
	if ( tag == CLEX::UNASS ){
	  cerr << "Unhandled class in rhs=" << rhs << endl;
	  continue;
	}
	else {
	  //	  cerr << "found tag '" << tag << "' in " << rhs << endl;
	  RightHand[i] = tag;
	  if ( tag == CLEX::AFFIX ){
	    fixpos = i;
	  }
	  if ( tag == CLEX::XAFFIX ){
	    xfixpos = i;
	  }
	}
      }
    }
  }
  else {
    //    cerr << "normal RulePart " << s << endl;
    CLEX::Type tag = CLEX::toCLEX( s[0] );
    string::size_type pos = s.find("/");
    if ( pos != string::npos ){
      // some inflextion
      if ( tag != CLEX::UNASS ){
	// cases like 0/e 0/te2I
	ResultClass = tag;
	inflect = s.substr(pos+1);
      }
      else {
	//  E/P
	inflect = s;
      }
      //      cerr << "inflect =" << inflect << endl;
    }
    else if ( tag != CLEX::UNASS ){
      // dull case
      ResultClass = tag;
    }
    else {
      // m
      inflect = s;
      //	cerr << "inflect =" << inflect << endl;
    }
  }
}

Rule::Rule( const vector<string>& parts,
	    const UnicodeString& s,
	    int flag ): debugFlag( flag ){
  for ( size_t k=0; k < parts.size(); ++k ) {
    string this_class = parts[k];
    RulePart cur( this_class, s[k] );
    rules.push_back( cur );
  }
}

ostream& operator<<( ostream& os, const Rule& r ){
  os << "MBMA rule:" << endl;
  for ( size_t k=0; k < r.rules.size(); ++k ) {
    os << "\t" << r.rules[k] << endl;
  }
  return os;
}

ostream& operator<<( ostream& os, const Rule *r ){
  if ( r )
    os << *r << endl;
  else
    os << "Empty MBMA rule" << endl;
  return os;
}

void Rule::reduceZeroNodes(){
  vector<RulePart> out;
  for ( size_t k=0; k < rules.size(); ++k ) {
    if ( rules[k].ResultClass == CLEX::NEUTRAL
	 && rules[k].morpheme.isEmpty()
	 && rules[k].inflect.empty() ){
    }
    else {
      out.push_back(rules[k]);
    }
  }
  rules.swap( out );
}

vector<string> Rule::extract_morphemes() const {
  vector<string> morphemes;
  vector<RulePart>::const_iterator it = rules.begin();
  while ( it != rules.end() ) {
    UnicodeString morpheme = it->morpheme;
    if ( !morpheme.isEmpty() ){
      morphemes.push_back( UnicodeToUTF8(morpheme) );
    }
    ++it;
  }
  return morphemes;
}
