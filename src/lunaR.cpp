
//    --------------------------------------------------------------------
//
//    This file is part of Luna.
//
//    LUNA is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    Luna is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Luna. If not, see <http://www.gnu.org/licenses/>.
//
//    Please see LICENSE.txt for more details.
//
//    --------------------------------------------------------------------


#include "lunaR.h"

extern globals global;

extern logger_t logger;

#include <iostream>

#include <string>

// a single, global instance

Rdata_t * rdata;

std::string Rversion()
{
  std::string vmaj = R_MAJOR;
  std::string vmin = R_MINOR;  
  return vmaj + "-" + vmin;
}

SEXP Rmake_string_vector( const std::vector<std::string> & r )
{
  SEXP v;
  PROTECT( v = Rf_allocVector( STRSXP, r.size()) );
  for (int i=0;i<r.size(); i++)
    SET_STRING_ELT(v, i, Rf_mkChar( r[i].c_str() ) );     
  UNPROTECT(1);
  return v;
}


void Rflush_log()
{
  Rprintf( logger.print_buffer().c_str() );
}

void R_init_luna(DllInfo *info)
{

  global.init_defs();

  // redefine error handler
  globals::bail_function = &R_bail_function;

  // turn off external output
  writer.nodb();
  
  // use this as a general store  
  rdata = NULL;
  
  // indicate that we are running inside R
  global.R( 0 ); // 0 means no log mirroring

  std::string msg = "** luna " ;
  msg += globals::version ;
  msg += " ";
  msg += globals::date;
  msg += "\n";
  
  Rprintf( msg.c_str() );
}


SEXP Rlogmode( SEXP i )
{
  std::vector<int> m = Rluna_to_intvector(i);
  if ( m.size() != 1 ) Helper::halt( "expecting a single integer 0 or 1 value" );
  global.R( m[0] );
  return( R_NilValue );
}



SEXP Rstat()
{

  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }


  // Record duration, as hh:mm:ss string
  uint64_t duration_tp = globals::tp_1sec
    * (uint64_t)rdata->edf.header.nr
    * rdata->edf.header.record_duration;
  
  std::string total_duration_hms = Helper::timestring( duration_tp );

  std::stringstream ss;
  
  int n_data_channels = 0;
  int n_annot_channels = 0;
  for (int i=0;i<rdata->edf.header.ns;i++) 
    {
      if ( rdata->edf.header.is_data_channel( i ) ) ++n_data_channels;
      else ++n_annot_channels;
    }

  ss << rdata->edf.id << " : "
     << n_data_channels << " signals, ";
  //  if ( n_annot_channels ) ss << n_annot_channels << " EDF annotations, ";
  ss   << rdata->edf.timeline.annotations.names().size() << " annotations, "
       << Helper::timestring( duration_tp ) << " duration";
  
  if ( rdata->edf.timeline.epoched() )
    {
      ss << ", " << rdata->edf.timeline.num_epochs() 
	 << " unmasked " << rdata->edf.timeline.epoch_length() << "-sec epochs, and " 
	 << rdata->edf.timeline.num_total_epochs() - rdata->edf.timeline.num_epochs()  << " masked";
    }
  
  ss << "\n";

  Rprintf( ss.str().c_str() );
  
  return( R_NilValue );

}

SEXP Rdesc()
{
  
  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }


  // EDF header and other information returned as an R list
  
  //1 EDF
  //2 ID
  //3 DUR
  //4 EPOCH_N
  //5 EPOCH_SEC

  //6 CH
  //   NAME
  //   FS

  //7 ANNOT
  //

  int protect = 0 ;
  
  SEXP r;
  PROTECT( r = Rf_allocVector( VECSXP, 11 )); 
  ++protect;
  
  SEXP r_names;
  PROTECT( r_names = Rf_allocVector( STRSXP, 11 )); 
  ++protect;
  
  SET_STRING_ELT( r_names , 0, Rf_mkChar( "EDF" ) );
  SET_STRING_ELT( r_names , 1, Rf_mkChar( "ID" ) );
  SET_STRING_ELT( r_names , 2, Rf_mkChar( "DUR" ) );
  SET_STRING_ELT( r_names , 3, Rf_mkChar( "SEC" ) );

  SET_STRING_ELT( r_names , 4, Rf_mkChar( "NS" ) );
  SET_STRING_ELT( r_names , 5, Rf_mkChar( "NS_ALL" ) );
  
  SET_STRING_ELT( r_names , 6, Rf_mkChar( "EPOCH_N" ) );
  SET_STRING_ELT( r_names , 7, Rf_mkChar( "EPOCH_N1" ) );
  SET_STRING_ELT( r_names , 8, Rf_mkChar( "EPOCH_SEC" ) );

  SET_STRING_ELT( r_names , 9, Rf_mkChar( "CHS" ) );
  SET_STRING_ELT( r_names , 10, Rf_mkChar( "ANNOTS" ) );	     
  Rf_setAttrib( r, R_NamesSymbol, r_names); 

  //
  // set data  
  //
  
  // EDF filename
  SET_VECTOR_ELT( r , 0, PROTECT( Rf_mkString( rdata->edf.filename.c_str() ) ) );
  ++protect;
  
  // ID
  SET_VECTOR_ELT( r , 1, PROTECT( Rf_mkString( rdata->edf.id.c_str() ) ) );
  ++protect;
  
  // Record duration, as hh:mm:ss string
  uint64_t duration_tp = globals::tp_1sec
    * (uint64_t)rdata->edf.header.nr
    * rdata->edf.header.record_duration;
  
  std::string total_duration_hms = Helper::timestring( duration_tp );

  SET_VECTOR_ELT( r , 2 , PROTECT( Rf_mkString( total_duration_hms.c_str() ) ) );
  ++protect;

  // Record duration, in seconds (double)
  SET_VECTOR_ELT( r , 3 , PROTECT( Rf_ScalarReal( (double) rdata->edf.header.nr
						  * rdata->edf.header.record_duration ) ) ) ;
  ++protect;
  
  // number of signals (in mem)
  SET_VECTOR_ELT( r , 4 , PROTECT( Rf_ScalarInteger( rdata->edf.header.ns ) ) ) ;
  ++protect;

  // number of signals (in file, i.e. total)
  SET_VECTOR_ELT( r , 5 , PROTECT( Rf_ScalarInteger( rdata->edf.header.ns_all ) ) ) ;
  ++protect;
  
  if ( rdata->edf.timeline.epoched() )
    {

      SET_VECTOR_ELT( r , 6 , PROTECT( Rf_ScalarInteger( rdata->edf.timeline.num_total_epochs() ) ) ) ;
      ++protect;
      
      SET_VECTOR_ELT( r , 7 , PROTECT( Rf_ScalarInteger( rdata->edf.timeline.num_epochs() ) ) ) ;
      ++protect;

      SET_VECTOR_ELT( r , 8 , PROTECT( Rf_ScalarInteger( rdata->edf.timeline.epoch_length() ) ) ) ;
      ++protect;

    }
  else
    {
      SET_VECTOR_ELT( r , 6 , PROTECT( Rf_ScalarInteger( NA_INTEGER ) ) ) ;
      ++protect;
      
      SET_VECTOR_ELT( r , 7 , PROTECT( Rf_ScalarInteger( NA_INTEGER ) ) ) ;
      ++protect;

      SET_VECTOR_ELT( r , 8 , PROTECT( Rf_ScalarInteger( NA_INTEGER ) ) ) ;
      ++protect;

    }


  //
  // Channels
  //
  
  const int ns = rdata->edf.header.ns;
  SEXP chs;
  PROTECT( chs = Rf_allocVector( VECSXP, ns )); 
  ++protect;

  SEXP chs_names;
  PROTECT( chs_names = Rf_allocVector( STRSXP, ns )); 
  ++protect;
  
  for (int s=0;s<ns;s++)
    {
      int fs = rdata->edf.header.n_samples[s];
      SET_VECTOR_ELT( chs , s , PROTECT( Rf_ScalarInteger( fs ) ) ) ;
      ++protect;
      
      SET_STRING_ELT( chs_names , s ,
		      PROTECT( Rf_mkChar( Helper::sanitize( rdata->edf.header.label[s] ) .c_str() ) ) ) ;
      ++protect;
      
    }
  
  Rf_setAttrib( chs, R_NamesSymbol, chs_names); 

  SET_VECTOR_ELT( r , 9 , chs );

  // Annotations
  
  const int na = 1;

  SEXP annots;
  PROTECT( annots = Rf_allocVector( VECSXP, na )); 
  ++protect;
  SEXP annots_names;
  PROTECT( annots_names = Rf_allocVector( STRSXP, na )); 
  ++protect;
  Rf_setAttrib( annots, R_NamesSymbol, annots_names); 

  SET_VECTOR_ELT( r , 10 , annots );

  // All done
  UNPROTECT( protect );
  return r;
      
}


void R_error( const std::string & s )
{
  Rf_error( s.c_str() ) ; 
} 


void R_warning( const std::string & s )
{
  Rf_warning( s.c_str() ) ; 
} 


//
// attach an EDF
//

SEXP Rattach_edf( SEXP x , SEXP id , SEXP ann )
{

  std::string edf_file = Helper::expand( CHAR( STRING_ELT( x , 0 ) ) );

  std::string edf_id   = CHAR( STRING_ELT( id , 0 ) );

  std::vector<std::string> annots = Rluna_to_strvector( ann );

  // check EDF exists
  if ( ! Helper::fileExists( edf_file ) )
    Helper::halt( "cannot find " + edf_file );

  // clear any old data
  if ( rdata != NULL ) delete rdata;
  
  rdata = new Rdata_t;
  
  bool okay = rdata->edf.attach( edf_file , edf_id );

  if ( ! okay ) 
    Helper::halt( "problem attaching EDF" + edf_file );

  // attach annotations
  for (int a=0;a<annots.size();a++)
    rdata->add_annotations( annots[a] );
  
  return(R_NilValue);
}


void Rclear()
{
  if ( rdata != NULL )
    {
      delete rdata;
      rdata = NULL;
    }
}

void Rclear_vars()
{
  cmd_t::vars.clear();
}

void Rset_var( SEXP x , SEXP y)
{
  std::vector<std::string> tok0 = Rluna_to_strvector(x);
  std::vector<std::string> tok1 = Rluna_to_strvector(y);
  if ( tok0.size() != tok1.size() ) Helper::halt( "problem setting variables" );
  for (int i=0;i<tok0.size();i++)
    {
      Rprintf( "setting [" );
      Rprintf( tok0[i].c_str() );
      Rprintf( "] to [" );
      Rprintf( tok1[i].c_str() );
      Rprintf( "]\n" );
      cmd_t::parse_special( tok0[i] ,tok1[i] );
    } 
}


//
// Attach annotations
//

bool Rdata_t::add_annotations( const std::string & annotfile )
{
  
  // is 'annotfile' in fact a folder (i.e. ending in '/') ? 
  
  if ( annotfile[ annotfile.size() - 1 ] == '/' )
    {

      // this means we are specifying a folder, in which case search for all files that                                                                             
      // start id_<ID>_* and attach thoses                                                                                                                          
      DIR * dir;
      struct dirent *ent;
      if ( (dir = opendir ( annotfile.c_str() ) ) != NULL )
	{
	  /* print all the files and directories within directory */
	  while ((ent = readdir (dir)) != NULL)
	    {
	      std::string fname = ent->d_name;
	      
	      if ( Helper::file_extension( fname , "ftr" ) ||
		   Helper::file_extension( fname , "xml" ) ||
		   Helper::file_extension( fname , "eannot" ) ||
		   Helper::file_extension( fname , "annot" ) )
		{
		  edf.load_annotations( annotfile + fname );
		}
	    }
	  closedir (dir);
	}
      else
	Helper::halt( "could not open folder " + annotfile );
    }

  //
  // else a single file, load it
  //

  else
   {
     edf.load_annotations( annotfile );
   }

  return true;
        
}



//
// evaluate a command
//

SEXP Reval_cmd( SEXP x )
{
  
  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }
  
  std::string cmdstr = CHAR( STRING_ELT( x , 0 ) );

  retval_t ret;

  writer.use_retval( &ret );

  // set ID 
  writer.id( rdata->edf.id , rdata->edf.filename);

  Rprintf( "evaluating...\n" );
  
  // set command string

  cmd_t cmd( cmdstr );
  
  // eval on the current EDF

  cmd.eval( rdata->edf );
  
  // swtich 'off' this stream for the next command
  // as it will get deleted on leaving this function
  
  writer.use_retval( NULL );
  
  // and we need to completely clear the writer (i.e. so that old
  // factor/level labels are not in place for the next run, etc)
  
  writer.clear();
  
  // convert retval_t to R list and return 

  return Rout_list( ret );
 
}


//
// pull a retval structure from an existing output database, for **1** individual
//

SEXP Rdb2retval( SEXP x , SEXP y )
{

  std::string dbstr = CHAR( STRING_ELT( x , 0 ) );

  // this can be empty if requesting all individuals from the DB
  std::vector<std::string> idstr = Rluna_to_strvector( y );
  
  std::set<std::string> idset;
  for (int i=0;i<idstr.size();i++) 
    idset.insert( idstr[i] );

  // this gets populated by the IDs actually read
  std::vector<std::string> ids;

  retval_t ret = writer_t::dump_to_retval( dbstr , &idset , &ids );

  std::string msg = "read data on " + Helper::int2str( (int)ids.size() ) + " individuals\n";
  Rprintf( msg.c_str() );
      
  // convert retval_t to R list and return 
  return Rout_list( ret );

}


//
// structure retval_t in list format for R
//

SEXP Rout_list( retval_t & r )
{

  // list [ cmd ]
  //    list [ table ]  e.g..  F_CH
  //            data.frame   :  cols = facs + vars  ; rows = lvls + values 

  // e.g.    F    CH   DENS  AMP
  //         11   C3   1.23  0.23
  //         16   C4   1.23  0.23

  // keep track of items on stack 
  int protect = 0;
	  
  // # of commands
  const int nc = r.data.size();
  
  SEXP c_list;
  PROTECT( c_list = Rf_allocVector( VECSXP, nc )); 
  ++protect;
  
  // command labels
  SEXP c_names;
  PROTECT( c_names = Rf_allocVector( STRSXP, nc )); 
  ++protect;
  
  int cmd_cnt = 0;

  // iterate over each command
  
  retval_data_t::iterator cc = r.data.begin();
  while ( cc != r.data.end() )
    {      
      
      const retval_cmd_t & cmd = cc->first;

      // number of virtual tables/factors for this command 
      
      const int nt = cc->second.size();
      
      SEXP t_list;
      PROTECT( t_list = Rf_allocVector( VECSXP, nt )); 
      ++protect;
      
      // command labels
      SEXP t_names;
      PROTECT( t_names = Rf_allocVector( STRSXP, nt )); 
      ++protect;
      
      int t_cnt = 0;
      
      // factors/tables
      
      std::map<retval_factor_t,
	std::map<retval_var_t,
	std::map<retval_strata_t,
	std::map<retval_indiv_t,
	retval_value_t > > > >::iterator tt = cc->second.begin();

      //std::cout << " (found " << nt << " tables)\n" ;

      
      while ( tt != cc->second.end() )
	{
	  
	  
	  const retval_factor_t & table = tt->first; 
	  
	  //std::cout << " considering table ...\n";
	  
	  //
	  // for this particular cmd/fac combination, make a data-frame
	  //
	  
	  //    cols = ID + factors + variables
	  //    rows = levels
	  //    items = values

	  // in this table ( tt ):
	  // how many factors (i.e. F CH == 2 )
	  // how many variables

	  // but we need to split these by type
	  
	  const int nf = table.factors.size();
	  
	  const int nv = tt->second.size();
	  
	  const int ncols = 1 + nf + nv;  // 1 for ID 

	  //std::cout << " has " << ncols << " cols, " << nf << " fac, " << nv << " vars\n";

	  //
	  // quickly scan for the a) number of rows
	  // and b) whether the factors are string, double or int
	  //

	  // str > dbl > int
	  std::set<std::string> int_factor, str_factor, dbl_factor;
	  
	  //
	  // to track rows, indiv/strata pairing
	  //
	  
	  
	  std::set<retval_indiv_strata_t> rows;
	  
	  std::map<retval_var_t,
	    std::map<retval_strata_t,
	    std::map<retval_indiv_t,
	    retval_value_t > > >::iterator vv = tt->second.begin();

	  while ( vv != tt->second.end() )
	    {

	      std::map<retval_strata_t, 
		std::map<retval_indiv_t,retval_value_t > >::iterator ss = vv->second.begin();
	      while ( ss != vv->second.end() )
		{
		  
		  const retval_strata_t & s = ss->first;
		  
		  // get rows (+ indivs)
		  std::map<retval_indiv_t,retval_value_t>::iterator ii = ss->second.begin();
		  while ( ii != ss->second.end() )
		    {
		      rows.insert( retval_indiv_strata_t( ii->first , s )  );
		      ++ii;
		    }
		      
		  // get factor types 
		  std::set<retval_factor_level_t>::iterator ll = s.factors.begin();
		  while ( ll != s.factors.end() )
		    {
		      if      ( ll->is_str ) str_factor.insert( ll->factor );
		      else if ( ll->is_dbl ) dbl_factor.insert( ll->factor );
		      else if ( ll->is_int ) int_factor.insert( ll->factor );
		      ++ll;
		    }
		      
		  ++ss;
		}
	      ++vv; 
	    }
	  
	  
	  //
	  // Now, we should know the number of rows, and whether a
	  // given factor is string, double or int
	  //
	  
	  const int nrows = rows.size();

	  //std::cout << " and found " << nrows << " rows\n";
	  
	  //
	  // we now need to build a matrix of 'nrows' rows and 'ncols' colums (fac + vars)
	  //
	  
	  SEXP df;     // data.frame	  
	  SEXP cls;    // class attribute
	  SEXP nam;    // col-names
	  SEXP rownam; // row-names
	  
	  // df = a list w/ ncols elements
	  PROTECT( df = Rf_allocVector( VECSXP, ncols ));
	  ++protect;
	  
	  // set class attribute for df
	  PROTECT(cls = Rf_allocVector(STRSXP, 1)); // class attribute
	  ++protect;
	  
	  SET_STRING_ELT(cls, 0, Rf_mkChar( "data.frame" ));
	  Rf_classgets(df, cls);
	  
	  // col-names
	  PROTECT(nam = Rf_allocVector(STRSXP, ncols)); // names attribute (column names)
	  ++protect;
	  

	  //
	  // Add ID as column 1 
	  //

	  int col_cnt = 0;
	  SEXP id_col;	      
	  PROTECT( id_col = Rf_allocVector( STRSXP , nrows ) );
	  ++protect;

	  // populate w/ IDs
	  // consider all indiv/factor/level rows
	  int r_cnt = 0;
	  std::set<retval_indiv_strata_t>::iterator rr =  rows.begin();
	  while ( rr != rows.end() )
	    {	      
	      retval_indiv_t indiv = rr->indiv;
	      SET_STRING_ELT( id_col,r_cnt,Rf_mkChar( rr->indiv.name.c_str() ) );
	      ++r_cnt;
	      ++rr;
	    }
	  
	  // add this column to the df
	  SET_VECTOR_ELT( df, col_cnt , id_col );
	  
	  // and add a name
	  SET_STRING_ELT(nam, col_cnt , Rf_mkChar( "ID" ));
	  
	  // shift to next col.
	  ++col_cnt;
	  
	  // and unprotect
	  UNPROTECT(1); --protect;

	  
	  //
	  // Add factors
	  //
	  
	  //std::cout << " wanting to create the data-frame...\n";
	  
	  std::set<std::string>::const_iterator ff = table.factors.begin();
	  while ( ff != table.factors.end() )
	    {

	      bool is_str_factor = false , is_dbl_factor = false , is_int_factor = true;

	      if ( str_factor.find( *ff ) != str_factor.end() )
		is_str_factor = true;
	      else if ( dbl_factor.find( *ff ) != dbl_factor.end() )
		is_dbl_factor = true;
	      
	      //std::cout << " fac -> " << ff->c_str() << "\n";
	      
	      SEXP col;
	      
	      if ( is_str_factor ) 
	       	PROTECT( col = Rf_allocVector( STRSXP , nrows ) );
	      else if ( is_dbl_factor )
	       	PROTECT( col = Rf_allocVector( REALSXP , nrows ) );
	      else
	       	PROTECT( col = Rf_allocVector( INTSXP , nrows ) );

	      ++protect;
	      
	      
	      // consider all indiv/factor/level rows
	      int r_cnt = 0;
	      std::set<retval_indiv_strata_t>::iterator rr =  rows.begin();
	      while ( rr != rows.end() )
		{
		  
		  //retval_indiv_t indiv = rr->indiv;
		  const retval_strata_t & strata = rr->strata;
		  const retval_factor_level_t & lvl = strata.find( *ff );
		  
		  // get value from 'fac', but bear in mind, it may
		  // be of different type (would be v. strange, but
		  // handle here just in case, w/ a cast)
		  
		  if ( is_str_factor )
		    {
		      if ( lvl.is_str ) 
			SET_STRING_ELT( col,r_cnt,Rf_mkChar( lvl.str_level.c_str() ) );
		      else if ( lvl.is_int )
			SET_STRING_ELT( col,r_cnt,Rf_mkChar( Helper::int2str(lvl.int_level).c_str() ) );
		      else if ( lvl.is_dbl )
			SET_STRING_ELT( col,r_cnt,Rf_mkChar( Helper::dbl2str(lvl.dbl_level).c_str() ) );
		    }
		  else if ( is_dbl_factor )
		    {
		      double d = lvl.dbl_level;
		      if ( lvl.is_str ) d = NA_REAL;
		      else if ( lvl.is_int ) d = lvl.int_level;

		      REAL(col)[ r_cnt ] = d;

		    }
		  else if ( is_int_factor )
		    {
		      int i = lvl.int_level;
		      if      ( lvl.is_str ) i = NA_INTEGER;
		      else if ( lvl.is_dbl ) i = (int)lvl.dbl_level;
		      
		      INTEGER(col)[r_cnt ] = i;

		    }
		  
		  //std::cout << "found " << lvl.print() << "\n";
		
		  ++r_cnt;
		  ++rr;
		}
	      
	      // add this column to the df
	      SET_VECTOR_ELT( df, col_cnt , col );

	      // and add a name
	      SET_STRING_ELT(nam, col_cnt , Rf_mkChar( ff->c_str() ));
	      
	      // clean up this one column 'col' added
	      
	      UNPROTECT(1);
	      --protect;

	      // next column (factor)
	      ++col_cnt;
	      
	      ++ff;
	    }
	

	  //
	  // Repeat, as for factors, but now adding actual variables 
	  //
		  
	  vv = tt->second.begin();
	  
	  while ( vv != tt->second.end() )
	    {

	      const retval_var_t & var = vv->first;
		
	      // what type of variable is this?
	      // vv->is_string(), vv->is_double(), vv->is_int()
	      
	      ///	      std::cout << " var -> " << var.name << "\n";
	      
	      bool var_is_string = r.var_has_strings.find( var.name ) != r.var_has_strings.end();
	      bool var_is_double = r.var_has_doubles.find( var.name ) != r.var_has_doubles.end();
	      
	      SEXP col;
	      
	      if      ( var_is_string )
	       	PROTECT( col = Rf_allocVector( STRSXP , nrows ) );
	      else if ( var_is_double )
	       	PROTECT( col = Rf_allocVector( REALSXP , nrows ) );
	      else
	       	//PROTECT( col = Rf_allocVector( INTSXP , nrows ) );  // because we might have long long ints returned, make everything REAL ...
   	        PROTECT( col = Rf_allocVector( REALSXP , nrows ) );
	      
	      ++protect;
	      	      
	      // consider all factor/level rows as before (based on same rows file)

	      int r_cnt = 0;

	      std::set<retval_indiv_strata_t>::iterator rr =  rows.begin();
	      while ( rr != rows.end() )
		{
		  
		  // i.e. we are ensuring that we are iterating in the same order as we
		  // previously did for each variable, so
		  
		  //
		  // does this variable have a non-missing value for this row/level, for this individual? 
		  //
		  
		  std::map<retval_strata_t, std::map<retval_indiv_t,retval_value_t> >::const_iterator yy = vv->second.find( rr->strata );

	          // not present...
	          if ( yy == vv->second.end() )
		    {
		      // set to NA
		      if      ( var_is_string ) SET_STRING_ELT( col,r_cnt, NA_STRING );
		      else if ( var_is_double ) REAL(col)[ r_cnt ] = NA_REAL;
		  //  else                      INTEGER(col)[ r_cnt ] = NA_INTEGER;
		      else                      REAL(col)[ r_cnt ] = NA_REAL; // as we might have long long ints returned
		      		      
		    }
		  else // ...is present as a strata... check for this individual
		    {
		      
		      std::map<retval_indiv_t,retval_value_t>::const_iterator zz = yy->second.find( rr->indiv );
		      
		      // not present...
		      if ( zz == yy->second.end() ) 
			{
			  // set to NA
			  if      ( var_is_string ) SET_STRING_ELT( col,r_cnt, NA_STRING );
			  else if ( var_is_double ) REAL(col)[ r_cnt ] = NA_REAL;
			  //  else                      INTEGER(col)[ r_cnt ] = NA_INTEGER;
			  else                      REAL(col)[ r_cnt ] = NA_REAL; // as we might have long long ints returned
			  
			}
		      else
			{

			  // because of how sqlite stores numeric values, a double may be cast as an int;
			  // therefore, some values for a double variable may in fact be stored as value.i (i.e. if 1.0, etc)
			  // therefore, we need to check for this special case, for data coming from db2retval at least
			  // (this will all be fine if coming from a luna eval() 
			  
			  if      ( var_is_string ) 
			    SET_STRING_ELT( col, r_cnt, Rf_mkChar( zz->second.s.c_str() ) );
			  else if ( var_is_double ) 
			    {
			      // special case
			      if ( zz->second.is_int )
				REAL(col)[ r_cnt ] = zz->second.i ;
			      else
				REAL(col)[ r_cnt ] = zz->second.d ;
			    }
			  else                      
			    //INTEGER(col)[ r_cnt ] = zz->second.i ; 
			    REAL(col)[ r_cnt ] = zz->second.i ;  // as we might have long long ints returned...
			}

		    }
	    
		  // next row/lvl
		  ++r_cnt;
		  ++rr;
   	       }
	      
	      // add this column to the df
	      SET_VECTOR_ELT( df, col_cnt , col );

	      // and add a name
	      SET_STRING_ELT(nam, col_cnt , Rf_mkChar( var.name.c_str() ));
	      
	      // clean up this one column 'col' added
	      
	      UNPROTECT(1);
	      --protect;

	      // next column (variable)
	      ++col_cnt;
	      
	      ++vv;
   	    }



	  //
	  // attach col-names to df
	  //
	  
	  Rf_namesgets(df, nam);
	  
	  
	  //
	  // row-names
	  //

	  PROTECT(rownam = Rf_allocVector(STRSXP, nrows)); // row.names attribute
	  ++protect;
	  for (int r = 0 ; r < nrows ; r++)
	    {
	      std::string rname = Helper::int2str( r+1 );
	      SET_STRING_ELT(rownam, r, Rf_mkChar( rname.c_str() ) );
	    }
	  Rf_setAttrib( df , R_RowNamesSymbol, rownam);

	  
	  // add this data-frame to the t_list (i.e. all tables for this command)
	  SET_VECTOR_ELT( t_list , t_cnt , df );

	  // label (factors, with _ delim)
	  std::string table_name = Helper::sanitize( Helper::stringize( table.factors , "_" ));
	  if ( table_name == "" ) table_name = "BL";
	  SET_STRING_ELT( t_names , t_cnt , Rf_mkChar( table_name.c_str() ));
	  	  
	  // Next virtual table
	  ++t_cnt;
	  ++tt;
	}

      // data
      SET_VECTOR_ELT( c_list , cmd_cnt , t_list );
      // names 
      SET_STRING_ELT( c_names , cmd_cnt , Rf_mkChar( Helper::sanitize( cmd.name).c_str() ));

      // set all t_list names      
      Rf_setAttrib( t_list, R_NamesSymbol, t_names); 
      
      ++cmd_cnt;
      ++cc;
      
    }

  // set c_list names

  Rf_setAttrib( c_list, R_NamesSymbol, c_names); 

  UNPROTECT( protect );
  return c_list;
}
  

SEXP Rmatrix_epochs( SEXP e , SEXP ch , SEXP ann )
{

  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }

  //
  // Signals: make signal_list_t and check sampling rates
  //

  std::string signal_label = Helper::stringize( Rluna_to_strvector( ch ) );
  
  signal_list_t signals = rdata->edf.header.signal_list( signal_label );  
  
  int fs = -1;
  
  for (int s=0; s< signals.size(); s++) 
    {      
      if ( rdata->edf.header.is_data_channel( s ) )
	{
	  if ( fs < 0 ) fs = rdata->edf.header.sampling_freq( signals(s) );
	  else if ( rdata->edf.header.sampling_freq( signals(s) ) != fs ) 
	    Helper::halt( "requires uniform sampling rate across signals" ); 	  
	}
    }



  //
  // Annotations
  //

  // 0 not found, 1 interval, 2 epoch
  std::map<std::string,int> atype; 
  
  std::vector<std::string> a = Rluna_to_strvector( ann );
  
  for (int i=0;i<a.size();i++)
    {
      if ( rdata->edf.timeline.annotations( a[i] ) != NULL ) // is this an interval annotation? 
	atype[ a[i] ] = 1;
      else if ( rdata->edf.timeline.epoch_annotation( a[i] ) ) // or an epoch-annotation?
	atype[ a[i] ] = 2;
      else
	atype[ a[i] ] = 0;	  
    }
  

  //
  // Epochs
  //  
  
  std::vector<int> epochs = Rluna_to_intvector( e );
  
  rdata->edf.timeline.ensure_epoched();
  
  int total_epochs = rdata->edf.timeline.num_epochs();
  
  // check epochs, given 1-based epoch count
  for (int epoch=0;epoch<epochs.size();epoch++)
    {      
      if ( epochs[epoch] < 1 || epochs[epoch] > total_epochs )
	Helper::halt( "invalid epoch number (expecting between 1 and "
		      + Helper::int2str( total_epochs ) + ")" );
    }


  std::vector<interval_t> epoch_intervals;
  for (int epoch=0;epoch<epochs.size();epoch++)
    {
      int epoch0 = epochs[epoch] - 1;
      
      interval_t interval = rdata->edf.timeline.epoch( epoch0 ); 
      
      epoch_intervals.push_back( interval );
    }
  
  
  //
  // Generate and return data.frame
  //
  
  return Rmatrix_internal( epoch_intervals, &epochs , signals, atype );

}


SEXP Rmatrix_intervals( SEXP i , SEXP ch , SEXP ann  )
{

  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }

  //
  // Signals: make signal_list_t and check sampling rates
  //

  std::string signal_label = Helper::stringize( Rluna_to_strvector( ch ) );
  
  signal_list_t signals = rdata->edf.header.signal_list( signal_label );  
  
  int fs = -1;
  
  for (int s=0; s< signals.size(); s++) 
    {      
      if ( rdata->edf.header.is_data_channel( s ) )
	{
	  if ( fs < 0 ) fs = rdata->edf.header.sampling_freq( signals(s) );
	  else if ( rdata->edf.header.sampling_freq( signals(s) ) != fs ) 
	    Helper::halt( "requires uniform sampling rate across signals" ); 	  
	}
    }
  


  //
  // Annotations
  //

  // 0 not found, 1 interval, 2 epoch
  std::map<std::string,int> atype; 
  
  std::vector<std::string> a = Rluna_to_strvector( ann );
  
  for (int i=0;i<a.size();i++)
    {
      if ( rdata->edf.timeline.annotations( a[i] ) != NULL ) // is this an interval annotation? 
	atype[ a[i] ] = 1;
      else if ( rdata->edf.timeline.epoch_annotation( a[i] ) ) // or an epoch-annotation?
	atype[ a[i] ] = 2;
      else
	atype[ a[i] ] = 0;	  
    }
  

  //
  // Intervals
  //  

  // expects a simple int vector where START1 STOP1 START2 STOP2 etc
  //   z <- as.numeric(rbind( i$START , i$STOP )

  
  std::vector<interval_t> intervals;
  
  std::vector<double> ints = Rluna_to_dblvector( i );
  if ( ints.size() % 2 ) Helper::halt( "internal error, expecting an even sized list");
  
  for (int e=0;e<ints.size();e+=2)
    {
      if ( ints[e+1] < ints[e] ) 
	Helper::halt( "internal error, expecting an even sized list"); 

      // as here the intervals are coming *from* R / the user, we can 
      // assume they will be in one-past-the-end format already
      interval_t interval( ints[e] * globals::tp_1sec , 
			   (ints[e+1] * globals::tp_1sec ) );

      intervals.push_back( interval );
    }

  //
  // Generate and return data.frame
  //
  
  return Rmatrix_internal( intervals, NULL, signals, atype );

}





//
// Do the actual work of pulling data out and making a data frame
//

SEXP Rmatrix_internal( const std::vector<interval_t> & intervals , 
		       const std::vector<int> * epoch_numbers , 
		       const signal_list_t & signals , 
		       const std::map<std::string,int> & atype )
		       

{
  
  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }


  //
  // function can *either* accept epochs or generic intervals
  // IF epoch_numbers is defined, this it must be the same length
  // as intervals, and we assume that these are the epoch numbers
  // (needed as we also display the old epoch number)
  //
  
  bool emode = epoch_numbers != NULL;
  
  if ( emode && intervals.size() != epoch_numbers->size() )
    Helper::halt( "internal error in Rmatrix_internal" );

  const int ni = intervals.size();
  const int na = atype.size();
  
  int ns = 0;
  for (int s = 0 ; s < signals.size() ; s++ )
    if ( rdata->edf.header.is_data_channel( signals(s) ) ) ++ns;


  //
  // Do we have any signals?  If not, use default_sample_rate specified as arg
  //

  if ( ns == 0 ) 
    Helper::halt( "requires at least one channel/data signal" );
  
  
  //
  // Point to first epoch
  //
  
  if ( emode && ! rdata->edf.timeline.epoched() ) 
    {
      int n = rdata->edf.timeline.set_epoch( globals::default_epoch_len , globals::default_epoch_len );
      logger << " set epochs to default " << globals::default_epoch_len << " seconds, " << n << " epochs\n";
      rdata->edf.timeline.first_epoch();
    }
  

  //
  // Create data.frame
  //

  // INT (+ E) + SEC + NS + NA 
  
  const int ncols = emode + 2  + ns + na;
  
  SEXP df;     // data.frame	  
  SEXP cls;    // class attribute
  SEXP nam;    // col-names
  SEXP rownam; // row-names
  
  int protect = 0;
  
  // df = a list w/ ncols elements
  PROTECT( df = Rf_allocVector( VECSXP, ncols ));
  ++protect;
  
  // set class attribute for df
  PROTECT(cls = Rf_allocVector(STRSXP, 1)); // class attribute
  ++protect;
  
  SET_STRING_ELT(cls, 0, Rf_mkChar( "data.frame" ));
  Rf_classgets(df, cls);
  
  // col-names
  PROTECT(nam = Rf_allocVector(STRSXP, ncols)); // names attribute (column names)
  ++protect;
  

  //
  // How many rows? Calculate this manually, by pulling the first signal... 
  //
  
  //
  // In general, we probably cannot assume we can calculate this based
  // on interval sizes and sample rates, as the intervals might not
  // fall cleanly on sample points i.e. the same interval could return
  // a different number of sample points depending on how it is
  // aligned with the sample points
  // 
  
    
  int nrows = 0;
    
  for (int i=0;i<ni;i++)
    {
      
      //
      // Interval (convert to 0-base)
      //
      
      const interval_t & interval = intervals[i];
      
      // first signal
      slice_t slice( rdata->edf , signals(0) , interval );
	  
      const std::vector<uint64_t> * tp = slice.ptimepoints();

      nrows += tp->size();

    }      
  
  // previous code: 
  // const int fs = rdata->edf.header.sampling_freq( signals(0) );
  // const int nrows_per_epoch = emode ? rdata->edf.timeline.epoch_length() * fs : 0 ;
  // if ( emode ) 
  //  nrows = ni * nrows_per_epoch;
  // else
  // {
  //  for (int i=0;i<intervals.size();i++)
  //   nrows += (int)(intervals[i].duration_sec() * fs) ; 
  //  }

  
  int col_cnt = 0;


  //
  // Interval strings
  //
  
  SEXP int_col;
  PROTECT( int_col = Rf_allocVector( STRSXP , nrows ) );
  ++protect;  


  //
  // Epochs
  //
  
  SEXP epoch_col;
  if ( emode )
    {
      PROTECT( epoch_col = Rf_allocVector( INTSXP , nrows ) );
      ++protect;  
    }

  //
  // Sec
  //

  SEXP sec_col;
  PROTECT( sec_col = Rf_allocVector( REALSXP , nrows ) );
  ++protect;


  //
  // Annots
  //
  
  std::vector<SEXP> annot_col(na);
  for (int a=0;a<na;a++) 
    {
      PROTECT( annot_col[a] = Rf_allocVector( INTSXP , nrows ) );
      ++protect;
    }
  
  //
  // Iterate over signals
  //

  for (int s=0; s<ns; s++) 
    {
      
      // create a new vector for the signal data
	  
      SEXP sig_col;
      PROTECT( sig_col = Rf_allocVector( REALSXP , nrows ) );
      ++protect;

      uint64_t row = 0;
      
      //
      // Consider each interval
      //
      
      for (int i=0;i<ni;i++)
	{
	  
	  //
	  // Interval (convert to 0-base)
	  //
	  
	  int epoch0 = emode ? (*epoch_numbers)[i] - 1 : 0 ; 
	  
	  const interval_t & interval = intervals[i];
	  
	  //
	  // Get data
	  //


	  slice_t slice( rdata->edf , signals(s) , interval );
	  
	  const std::vector<double> * data = slice.pdata();
	  
	  const std::vector<uint64_t> * tp = slice.ptimepoints();

	  int nrows_per_interval = tp->size();
 
//int nrows_per_interval = emode ? nrows_per_epoch :  (int)(interval.duration_sec() * fs ) ;
	  
	  // we don't need to do this now, as we've manually calculated the expected
	  // length above

	  // check as expected; we might expect this is off by one or so due to 
	  // rounding and imprecision in how the data are stored
// 	  if ( data->size() != nrows_per_interval ) 
// 	    Helper::halt( "internal error in matrix_internal: " 
// 			  + Helper::int2str( (int)data->size() ) 
// 			  + " " + Helper::int2str( nrows_per_interval ) ) ;
	  
	  //
	  // Populate signals
	  //
	  
	  for (int r=0;r<nrows_per_interval;r++)
	    {
	      
	      // Only add E/SEC and annotations once
	      if ( col_cnt == 0 ) 
		{		    
		  
		  // interval label
		  SET_STRING_ELT( int_col , row  , Rf_mkChar( interval.as_string().c_str() ) );

		  // epoch number
		  if ( emode )
		    INTEGER(epoch_col)[ row ] = (*epoch_numbers)[i];

		  // elapsed time in seconds
		  REAL(sec_col)[ row ] = (*tp)[r] * globals::tp_duration ;
		  
		  // Annotations (0/1) E,S 
		  
		  int a_col = 0;
		  
		  std::map<std::string,int>::const_iterator aa = atype.begin();
		  while ( aa != atype.end() )
		    {
		      
		      if ( aa->second == 0 )
			INTEGER(annot_col[a_col])[row] = NA_INTEGER;
		      else if ( aa->second == 1 )
			{
			  // get exact point      
			  interval_t interval2 = interval_t( (*tp)[r] , (*tp)[r] + 1LLU );
			  annot_t * annot = rdata->edf.timeline.annotations( aa->first );
			  annot_map_t events = annot->extract( interval2 );
			  bool has_annot = events.size() ;
			  INTEGER(annot_col[a_col])[row] = (int)(has_annot ? 1 : 0 );
			}
		      else if ( aa->second == 2 )
			INTEGER(annot_col[a_col])[row] = (int)( rdata->edf.timeline.epoch_annotation( aa->first , epoch0 ) ? 1 : 0 ) ;
		      
		      // next annotation
		      ++a_col;
		      ++aa;
		      
		    }
		
		}
	      
	      // Signal data
	      
	      REAL(sig_col)[ row ] = (*data)[r];
	      
	      // next row 
	      
	      ++row;
	    }
	  
	  //
	  // Next epoch
	  //
	}
      

      //
      // Attach cols to df
      //

      if ( col_cnt == 0 ) 
	{	  

	  SET_VECTOR_ELT( df, 0 , int_col );
	  UNPROTECT(1); --protect;

	  if ( emode )
	    {
	      SET_VECTOR_ELT( df, 1 , epoch_col );
	      UNPROTECT(1); --protect;
	    }

	  SET_VECTOR_ELT( df, emode + 1 , sec_col );
	  UNPROTECT(1); --protect;

	  // headers
	  SET_STRING_ELT(nam, 0 , Rf_mkChar( "INT" ) );
	  if ( emode ) SET_STRING_ELT(nam, 1 , Rf_mkChar( "E" ) );
	  SET_STRING_ELT(nam, emode+1 , Rf_mkChar( "SEC" ) );
	  
	  // annots
	  int acol1 = emode + 2 + ns;
	  int acol0 = 0;
	  std::map<std::string,int>::const_iterator aa = atype.begin();
	  while ( aa != atype.end() )
	    {
	      SET_VECTOR_ELT( df , acol1 , annot_col[ acol0 ] );
	      UNPROTECT(1); --protect;	      
	      
	      SET_STRING_ELT(nam, acol1 , Rf_mkChar( Helper::sanitize( aa->first ).c_str() ) );
	      ++acol1; ++acol0;
	      ++aa;
	    }
	  
	}
    

      // header
      std::string signal_name = rdata->edf.header.label[ signals(s) ] ;
      SET_STRING_ELT(nam, col_cnt + emode+2 , Rf_mkChar( Helper::sanitize( signal_name).c_str() ));
      UNPROTECT(1); --protect;

      // data
      SET_VECTOR_ELT( df, col_cnt + 2+emode , sig_col );
      UNPROTECT(1); --protect;

      
      //
      // Next signal
      //

      ++col_cnt;
      
    }


   
  
  //
  // Attach col-names to df
  //
  
  Rf_namesgets(df, nam);
    

  //
  // row-names
  //
  
  PROTECT(rownam = Rf_allocVector(STRSXP, nrows)); // row.names attribute
  ++protect;
  for (int r = 0 ; r < nrows ; r++)
    {
      std::string rname = Helper::int2str( r+1 );
      SET_STRING_ELT(rownam, r, Rf_mkChar( rname.c_str() ) );
    }
  Rf_setAttrib( df , R_RowNamesSymbol, rownam);
  
  
  UNPROTECT( protect );
	  
  return df;

}






SEXP Rchannels() 
{
  // data channels only
  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }
  
  // use signal 
  signal_list_t signals = rdata->edf.header.signal_list( "*" );
  std::vector<std::string> labels;
  const int ns = signals.size();
  for (int s=0;s<ns;s++) 
    if ( rdata->edf.header.is_data_channel( signals(s) ) ) 
      labels.push_back( signals.label(s) );
  return Rmake_string_vector( labels );
  
}  



SEXP Rmask( SEXP ann ) 
{

  //
  // Check we have an attached EDF
  //

  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }


  //
  // Any annotations to deal with?
  // 

  std::vector<std::string> annots = Rluna_to_strvector( ann );

  std::map<std::string,int> atype;
  
  for (int a=0;a<annots.size();a++)
    {
      if ( rdata->edf.timeline.annotations( annots[a] ) != NULL ) // is this an interval annotation? 
	atype[ annots[a] ] = 1; 
      else if ( rdata->edf.timeline.epoch_annotation( annots[a] ) ) // or an epoch-annotation?
	atype[ annots[a] ] = 2; 
    }
  
  const int na = atype.size();
  

  //
  // Set up return matrix
  //
  
  // number of rows = epochs
  const int ne = rdata->edf.timeline.num_total_epochs();

  if ( ne == 0 )
    {
      R_error( "no epochs defined" );
      return( R_NilValue );
    }
  
  SEXP df;     // data.frame	  
  SEXP cls;    // class attribute
  SEXP nam;    // col-names
  SEXP rownam; // row-names
  
  // E E0 ELAPSED SEC HMS M + (annots)
  
  const int ncols = 6 + na ;

  int protect = 0;

  // df = a list w/ ncols elements
  PROTECT( df = Rf_allocVector( VECSXP, ncols ));
  ++protect;
  
  // set class attribute for df
  PROTECT(cls = Rf_allocVector(STRSXP, 1)); 
  ++protect;
  SET_STRING_ELT(cls, 0, Rf_mkChar( "data.frame" ));
  Rf_classgets(df, cls);
  
  // col-names  
  PROTECT(nam = Rf_allocVector(STRSXP, ncols)); 
  ++protect;
    
  SEXP col_e;
  PROTECT( col_e = Rf_allocVector( INTSXP , ne ) );
  ++protect;

  SEXP col_e0;
  PROTECT( col_e0 = Rf_allocVector( INTSXP , ne ) );
  ++protect;
  
  SEXP col_sec0;
  PROTECT( col_sec0 = Rf_allocVector( REALSXP , ne ) );
  ++protect;

  SEXP col_sec;
  PROTECT( col_sec = Rf_allocVector( REALSXP , ne ) );
  ++protect;
  
  SEXP col_hms;
  PROTECT( col_hms = Rf_allocVector( STRSXP , ne ) );
  ++protect;
  
  SEXP col_m;
  PROTECT( col_m = Rf_allocVector( INTSXP , ne ) );
  ++protect;


  // annots

  std::vector<SEXP> col_annots(na);
  std::map<std::string,int>::const_iterator aa = atype.begin();
  int acnt = 0;
  while ( aa != atype.end() )
    {
      PROTECT( col_annots[acnt] = Rf_allocVector( INTSXP , ne ) );      
      ++protect;
      ++acnt;
      ++aa;      
    }
  

  rdata->edf.timeline.first_epoch();
  
  // elapsed (unmasked) epochs
  int ee = 0;
  int unmasked_ee = 0;
  
  clocktime_t starttime( rdata->edf.header.starttime );

  bool hms = starttime.valid;
  
  while ( 1 ) 
    {
      int epoch = rdata->edf.timeline.next_epoch_ignoring_mask();
      
      if ( epoch == -1 ) break;
      
      const bool masked = rdata->edf.timeline.masked( epoch );

      const int epoch0 = rdata->edf.timeline.display_epoch( epoch );
      
      const double sec = unmasked_ee * rdata->edf.timeline.epoch_length();
      
      interval_t interval = rdata->edf.timeline.epoch( epoch );
      
      const double sec0 = interval.start * globals::tp_duration;
      
      clocktime_t present = starttime;
      present.advance( sec0 / 3600.0 );
      
      std::string clocktime = present.as_string();

      if ( masked ) 
	{
	  INTEGER( col_e )[ ee ] = NA_INTEGER;
	  REAL( col_sec )[ ee ] = NA_REAL;
	}
      else
	{
	  INTEGER( col_e )[ ee ] = unmasked_ee + 1 ; // make output 1-based
	  REAL( col_sec )[ ee ] = sec;
	}

      INTEGER( col_e0 )[ ee ] = epoch0;
      REAL( col_sec0 )[ ee ] = sec0;      
      
      if ( hms )
	SET_STRING_ELT( col_hms ,ee , Rf_mkChar( clocktime.c_str() ) );
      else
	SET_STRING_ELT( col_hms ,ee , NA_STRING );

      INTEGER( col_m )[ ee ] = masked;


      //
      // Annotations
      //

      int acnt = 0;
      
      std::map<std::string,int>::const_iterator aa = atype.begin();
      while ( aa != atype.end() )
	{
	  
	  if ( aa->second == 1 ) 
	    {		  
	      annot_t * annot = rdata->edf.timeline.annotations( aa->first );
	      annot_map_t events = annot->extract( interval );
	      bool has_annot = events.size() ;
	      INTEGER( col_annots[acnt] )[ ee ] = ( has_annot ? 1 : 0 ) ;
	    }
	  else if ( aa->second == 2 ) 
	    {
	      INTEGER( col_annots[acnt] )[ ee ] = rdata->edf.timeline.epoch_annotation( aa->first , epoch ) ? 1 : 0 ;
	    }
	  ++acnt;
	  ++aa;
	}


      

      //
      // Next epoch
      //

      if ( ! masked ) ++unmasked_ee;

      ++ee;

    }
  
  
  //
  // attach cols to df
  //
  
  SET_VECTOR_ELT( df, 0 , col_e );
  SET_VECTOR_ELT( df, 1 , col_sec );
  SET_VECTOR_ELT( df, 2 , col_e0 );
  SET_VECTOR_ELT( df, 3 , col_sec0 );
  SET_VECTOR_ELT( df, 4 , col_hms );
  SET_VECTOR_ELT( df, 5 , col_m );
  for (int a=0;a<na;a++)
    SET_VECTOR_ELT( df, 6+a, col_annots[a] );

  //
  // attach col-names to df
  //
  
  SET_STRING_ELT(nam, 0, Rf_mkChar( "E" ));  
  SET_STRING_ELT(nam, 1, Rf_mkChar( "SEC" ));  
  SET_STRING_ELT(nam, 2, Rf_mkChar( "E1" ));  
  SET_STRING_ELT(nam, 3, Rf_mkChar( "SEC1" ));
  SET_STRING_ELT(nam, 4, Rf_mkChar( "HMS" ));
  SET_STRING_ELT(nam, 5, Rf_mkChar( "M" ));

  aa = atype.begin();
  int a = 0;
  while ( aa != atype.end() )
    {
      SET_STRING_ELT(nam, 6+a, Rf_mkChar( Helper::sanitize( aa->first ).c_str() ));
      ++a;
      ++aa;
    }
  Rf_namesgets(df, nam);
  
  
  //
  // row-names
  //
  
  PROTECT(rownam = Rf_allocVector(STRSXP, ne)); // row.names attribute
  ++protect;
  for (int r = 0 ; r < ne ; r++)
    {
      std::string rname = Helper::int2str( r+1 );
      SET_STRING_ELT(rownam, r, Rf_mkChar( rname.c_str() ) );
    }
  Rf_setAttrib( df , R_RowNamesSymbol, rownam);
  
  // release resources
  UNPROTECT( protect );
  
  return( df );

}  


SEXP Rannots() 
{

  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }

  return Rmake_string_vector( rdata->edf.timeline.annotations.names() );

}




std::vector<std::string> Rluna_to_strvector( SEXP x )
{
  const int n = Rf_length( x );
  std::vector<std::string> v(n);
  for (int i=0;i<n;i++) v[i] = CHAR( STRING_ELT( x, i ) );
  return v;
}


std::vector<int> Rluna_to_intvector( SEXP x )
{
  const int n = Rf_length( x );
  std::vector<int> v(n);
  if ( TYPEOF( x ) == INTSXP )
    for (int i=0;i<n;i++) v[i] = INTEGER(x)[i];
  return v;
}


std::vector<double> Rluna_to_dblvector( SEXP x )
{
  const int n = Rf_length( x );
  std::vector<double> v(n);
  if ( TYPEOF( x ) == REALSXP )
    for (int i=0;i<n;i++) v[i] = REAL(x)[i];
  return v;
}








//
// apply a function over epochs
//

SEXP Riterate( SEXP fn , SEXP ch , SEXP ann , SEXP byannot , SEXP w, SEXP rho )
{
  
  //
  // Requires an attached EDF
  //
  
  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }
 
  //
  // are we iterating by annotation or by epoch?
  //
  
  bool by_annotation = Rf_length( byannot ) == 1 ; 
  

  if ( Rf_length( byannot ) == 2 ) 
    Helper::halt( "only a single annotation can be specified with by.annot");

  //
  // Windows around annotations (in seconds)
  //

  double window = 0;

  if ( Rf_length(w) == 1 ) 
    window = REAL(w)[0];
  
  
  //
  // validate input
  //
  
  if ( ! Rf_isFunction(fn) ) 
    Helper::halt("'fn' must be a function");
  
  if ( ! Rf_isEnvironment(rho)) 
    Helper::halt("'rho' should be an environment");

  SEXP R_fcall;

  PROTECT(R_fcall = Rf_lang2(fn, R_NilValue));                                                                                                                   
  // EDF must be epoched

  if ( (! by_annotation ) && ! rdata->edf.timeline.epoched() ) 
    Helper::halt( "data not epoched" );
  

  //
  // Signals: make signal_list_t and check sampling rates
  //

  std::string signal_label = Helper::stringize( Rluna_to_strvector( ch ) );
  
  signal_list_t signals = rdata->edf.header.signal_list( signal_label );  
  
  int fs = -1;
  
  for (int s=0; s< signals.size(); s++) 
    {      
      if ( rdata->edf.header.is_data_channel( s ) )
	{
	  if ( fs < 0 ) fs = rdata->edf.header.sampling_freq( signals(s) );
	  else if ( rdata->edf.header.sampling_freq( signals(s) ) != fs ) 
	    Helper::halt( "requires uniform sampling rate across signals" ); 	  
	}
    }
  

  //
  // Annotations
  //

  // 0 not found, 1 interval, 2 epoch
  std::map<std::string,int> atype; 
  
  std::vector<std::string> a = Rluna_to_strvector( ann );
  
  for (int i=0;i<a.size();i++)
    {
      if ( rdata->edf.timeline.annotations( a[i] ) != NULL ) // is this an interval annotation? 
	atype[ a[i] ] = 1;
      else if ( rdata->edf.timeline.epoch_annotation( a[i] ) ) // or an epoch-annotation?
	atype[ a[i] ] = 2;
      else
	atype[ a[i] ] = 0;	  
    }
  
  //
  // Iterate over annotations 
  //
  
  if ( by_annotation )
    {

      //
      // Get annotation intervals
      //
      
      const std::string annot_class =  CHAR( STRING_ELT( byannot , 0 ) );
      
      //
      // get annotations
      //
      
      annot_t * annot = rdata->edf.timeline.annotations.find( annot_class );
      
      int na = 0;
  
      if ( annot == NULL ) na = 0;
      else na = annot->interval_events.size();
            
      if ( na != 0 && annot != NULL )
	{

	  // iterate over annotations
	  int cnt = 0;
	  annot_map_t::const_iterator ii = annot->interval_events.begin();
	  while ( ii != annot->interval_events.end() )
	    {
	      const instance_idx_t & instance_idx = ii->first;      
	      interval_t interval = instance_idx.interval;
	      
	      // add window?
	      if ( window > 0 ) 
		interval.expand( window * globals::tp_1sec );
	      
	      // progress report
	      ++cnt;
	  
	      if ( cnt % 40 == 0 ) Rprintf( (". " + Helper::int2str(cnt)+" epochs\n").c_str() );       
	      else Rprintf( "." );
	      
	      // check for user interupt as this may be long-running...
	      R_CheckUserInterrupt();

	      // Get data for this set
	      std::vector<interval_t> aints(1);
	      aints[0] = interval;
	  
	      SEXP edata;
	      
	      PROTECT( edata = Rmatrix_internal( aints, NULL , signals , atype ) );
	  
	      // bind parameter 
	      
	      SETCADR( R_fcall, edata );
	  
	      // Evalute function
	      
	      Rf_eval( R_fcall , rho);
	  
	      // all done
	      
	      UNPROTECT(1);    
	  
	      // next annotation interval
	      ++ii;
	      
	    }
	  
	  Rprintf( (" "+Helper::int2str(cnt)+ " intervals, done\n").c_str() );
	  
	}
      
    }
  
  //
  // Iterate over epochs
  //

  else
    {

      rdata->edf.timeline.first_epoch();
      
      int cnt = 0;
      while ( 1 ) 
	{
	  int epoch = rdata->edf.timeline.next_epoch();
	  
	  if ( epoch == -1 ) break;
	  
	  ++cnt;
	  
	  if ( cnt % 40 == 0 ) Rprintf( (". " + Helper::int2str(cnt)+" epochs\n").c_str() );       
	  else Rprintf( "." );
	  
	  // check for user interupt as this may be long-running...
	  R_CheckUserInterrupt();
	  
	  // Get data for this set
	  std::vector<int> epochs(1);
	  epochs[0] = epoch+1; // R expects 1-based inputs...
	  std::vector<interval_t> eints(1);
	  eints[0] = rdata->edf.timeline.epoch( epoch );
	  
	  SEXP edata;
	  
	  PROTECT( edata = Rmatrix_internal( eints, &epochs , signals , atype ) );
	  
	  // bind parameter 
	  
	  SETCADR( R_fcall, edata );
	  
	  // Evalute function
	  
	  Rf_eval( R_fcall , rho);
	  
	  // all done
	  
	  UNPROTECT(1);    
	  
	}
      
      Rprintf( (" "+Helper::int2str(cnt)+ " epochs, done\n").c_str() );

    }


  //
  // All done
  //

  UNPROTECT(1);    

  return( R_NilValue );
}



SEXP Rannot( SEXP ann )
{
  if ( rdata == NULL )
    {
      R_error( "no EDF attached" );
      return( R_NilValue );
    }
  
  const std::string annot_class =  CHAR( STRING_ELT( ann , 0 ) );
  

  //
  // get annotations
  //

  annot_t * annot = rdata->edf.timeline.annotations.find( annot_class );

  int na = 0;
  
  if ( annot == NULL ) na = 0;
  else na = annot->interval_events.size();

  // nothing to return?
  if ( na == 0 ) return( R_NilValue );


  //
  // return list( A1 [ START , STOP ] , A2 = [ START STOP ] ... )
  //


  int protect = 0 ; 

  SEXP c_list;
  PROTECT( c_list = Rf_allocVector( VECSXP, na )); 
  ++protect;
  
  // command labels
//   SEXP c_names;
//   PROTECT( c_names = Rf_allocVector( STRSXP, na )); 
//   ++protect;

  // START and STOP are defined as FIRST AND ONE-PAST-END of last element

//   SET_STRING_ELT( c_names , 0 , Rf_mkChar( "START" ) );
//   SET_STRING_ELT( c_names , 1 , Rf_mkChar( "STOP" ) );


  if ( annot != NULL )
    {
      // iterate over annotations
      int acnt = 0;
      annot_map_t::const_iterator ii = annot->interval_events.begin();
      while ( ii != annot->interval_events.end() )
	{
	  
	  // intervals
	  SEXP s1;
	  PROTECT( s1 = Rf_allocVector( REALSXP, 2 )); 
	  ++protect;
	  
	  const instance_idx_t & instance_idx = ii->first;      
	  REAL(s1)[0] = instance_idx.interval.start * globals::tp_duration ;
	  REAL(s1)[1] = instance_idx.interval.stop * globals::tp_duration;
	  // keeps 1-past-the-end encoding

	  // attach to main list
	  SET_VECTOR_ELT( c_list , acnt , s1 );
	  
	  UNPROTECT(1);
	  --protect;

	  // next
	  ++acnt;
	  ++ii;
	}
    }

  // set c_list names
  //  Rf_setAttrib( c_list, R_NamesSymbol, c_names); 

  UNPROTECT( protect );
  return c_list;

  
}

