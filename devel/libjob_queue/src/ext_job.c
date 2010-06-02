#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <util.h>
#include <hash.h>
#include <ext_job.h>
#include <config.h>
#include <stringlist.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <subst_list.h>
#include <parser.h>

/*
  About arguments
  ---------------
  How a job is run is defined in terms of the following variables:

   o stdout_file / stdin_file / stderr_file
   o arglist
   o ....

  These variables will then contain string values from when the job
  configuration is read in, for example this little job 

    STDOUT   my_stdout
    STDERR   my_stderr
    ARGLIST  my_job_input   my_job_output

  stdout & stderr are redirected to the files 'my_stdout' and
  'my_stderr' respectively, and when invoked with an exec() call the
  job is given the argumentlist:

       my_job_input   my_job_output

  This implies that _every_time_ this job is invoked the argumentlist
  will be identical; that is clearly quite limiting! To solve this we
  have the possibility of performing string substitutions on the
  strings in the job defintion prior to executing the job, this is
  handled with the privat_args substitutions. The definition for a
  copy-file job:


    EXECUTABLE   /bin/cp
    ARGLIST      <SRC_FILE>  <TARGET_FILE>


  This can then be invoked several times, with different key=value
  arguments for the SRC_FILE and TARGET_FILE:
 
  
      COPY_FILE(SRC_FILE = file1 , TARGET_FILE = /tmp/file1)
      COPY_FILE(SRC_FILE = file2 , TARGET_FILE = /tmp/file2)

*/



/*
 

jobList = [
    {"executable"  : None, 
     "environment" : {"LM_LICENSE_PATH" : "1700@osl001lic.hda.hydro.com:1700@osl002lic.hda.hydro.com:1700@osl003lic.hda.hydro.com",
                      "F_UFMTENDIAN"    : "big"},
     "target_file":"222",
     "argList"   : [],
     "stdout"    : "eclipse.stdout",
     "stderr"    : "eclipse.stdout",
     "stdin"     : "eclipse.stdin"}]
*/


#define EXT_JOB_TYPE_ID 763012


struct ext_job_struct {
  UTIL_TYPE_ID_DECLARATION;
  char       	  * name;
  char 	     	  * executable;
  char 	     	  * target_file;
  char       	  * start_file;            /* Will not start if not this file is present */
  char 	     	  * stdout_file;
  char 	     	  * stdin_file;
  char 	     	  * stderr_file;
  char       	  * lsf_resources;  
  char            * license_path;          /* If this is NULL - it will be unrestricted ... */
  char            * license_root_path;     
  char            * config_file; 
  int               max_running;           /* 0 means unlimited. */ 
  int               max_running_minutes;   /* The maximum number of minutes this job is allowed to run - 0: unlimited. */
  subst_list_type * private_args;          /* A substitution list of input arguments which is performed before the external substitutions - 
                                              these are the arguments supplied as key=value pairs in the forward model call. */
  char            * argv_string;
  stringlist_type * argv;                  /* This should *NOT* start with the executable */
  hash_type  	  * environment;
  
  bool              private_job;           /* Can the current user/delete this job? (private_job == true) means the user can edit it. */
  bool              __valid;               /* Temporary variable consulted during the bootstrap - when the ext_job is completely initialized this should NOT be consulted anymore. */
};


static UTIL_SAFE_CAST_FUNCTION( ext_job , EXT_JOB_TYPE_ID)





static ext_job_type * ext_job_alloc__(const char * name , const char * license_root_path , bool private_job) {
  ext_job_type * ext_job = util_malloc(sizeof * ext_job , __func__);
  
  UTIL_TYPE_ID_INIT( ext_job , EXT_JOB_TYPE_ID);
  ext_job->name                = util_alloc_string_copy( name );
  ext_job->license_root_path   = util_alloc_string_copy( license_root_path );
  ext_job->executable          = NULL;
  ext_job->stdout_file         = NULL;
  ext_job->target_file         = NULL;
  ext_job->start_file          = NULL;
  ext_job->stdin_file          = NULL;
  ext_job->stderr_file         = NULL;
  ext_job->argv 	       = NULL;
  ext_job->lsf_resources       = NULL;
  ext_job->environment         = hash_alloc();
  ext_job->argv                = stringlist_alloc_new();
  ext_job->argv_string         = NULL;
  ext_job->__valid             = true;
  ext_job->license_path        = NULL;
  ext_job->config_file         = NULL;
  ext_job->max_running         = 0;                  /* 0 means unlimited. */
  ext_job->max_running_minutes = 0;                  /* 0 means unlimited. */
  ext_job->private_job         = private_job;        /* If private_job == true the job is user editable. */ 

  /* 
     ext_job->private_args is set explicitly in the ext_job_alloc() 
     and ext_job_alloc_copy() functions. 
  */
  return ext_job;
}



/* 
   Exported function - must have name != NULL. Observe that the
   instance returned from this function is not really usable for
   anything.

   Should probably define a minium set of parameters which must be set
   before the job is in a valid initialized state.
*/

ext_job_type * ext_job_alloc(const char * name , const char * license_root_path , bool private_job) {
  ext_job_type * ext_job = ext_job_alloc__(name , license_root_path , private_job);
  ext_job->private_args  = subst_list_alloc( NULL );
  return ext_job;
}




ext_job_type * ext_job_alloc_copy(const ext_job_type * src_job) {
  ext_job_type * new_job  = ext_job_alloc__( src_job->name , src_job->license_root_path , true /* All copies are by default private jobs. */);
  
  new_job->config_file    = util_alloc_string_copy(src_job->config_file);
  new_job->executable     = util_alloc_string_copy(src_job->executable);
  new_job->target_file    = util_alloc_string_copy(src_job->target_file);
  new_job->start_file     = util_alloc_string_copy(src_job->start_file);
  new_job->stdout_file    = util_alloc_string_copy(src_job->stdout_file);
  new_job->stdin_file     = util_alloc_string_copy(src_job->stdin_file);
  new_job->stderr_file    = util_alloc_string_copy(src_job->stderr_file);
  new_job->lsf_resources  = util_alloc_string_copy(src_job->lsf_resources);  
  new_job->license_path   = util_alloc_string_copy(src_job->license_path);  
  
  if (src_job->argv      != NULL) new_job->argv          = stringlist_alloc_deep_copy( src_job->argv );

  new_job->max_running_minutes   = src_job->max_running_minutes;
  new_job->max_running           = src_job->max_running;
  new_job->private_args          = subst_list_alloc_deep_copy( src_job->private_args );
  /* Copying over all the keys in the environment hash table */
  {
    hash_iter_type * iter     = hash_iter_alloc( src_job->environment );
    const char * key = hash_iter_get_next_key(iter);
    while (key != NULL) {
      char * value = hash_get( src_job->environment , key);
      hash_insert_hash_owned_ref( new_job->environment , key , util_alloc_string_copy(value) , free);
      key = hash_iter_get_next_key(iter);
    }
    hash_iter_free(iter); 
  }
  
  return new_job;
}




void ext_job_free(ext_job_type * ext_job) {
  free(ext_job->name);
  util_safe_free(ext_job->executable);
  util_safe_free(ext_job->stdout_file);
  util_safe_free(ext_job->stdin_file);
  util_safe_free(ext_job->target_file);
  util_safe_free(ext_job->stderr_file);
  util_safe_free(ext_job->lsf_resources);
  util_safe_free(ext_job->license_path);
  util_safe_free(ext_job->license_root_path);
  util_safe_free(ext_job->config_file);
  util_safe_free(ext_job->argv_string);
  
  hash_free( ext_job->environment );
  
  if (ext_job->argv != NULL)         stringlist_free(ext_job->argv);
  
  subst_list_free( ext_job->private_args );
  free(ext_job);
}

void ext_job_free__(void * __ext_job) {
  ext_job_free ( ext_job_safe_cast(__ext_job) );
}


static void __update_mode( const char * filename , mode_t add_mode) {
  if (util_addmode_if_owner( filename , add_mode))
    printf("Updated mode on \'%s\'.\n", filename );
}


/**
   The license_path = 
   
   root_license_path / job_name / job_name 

*/

static void ext_job_init_license_control(ext_job_type * ext_job) {
  if (ext_job->license_path == NULL) {
    ext_job->license_path   = util_alloc_sprintf("%s%c%s" , ext_job->license_root_path , UTIL_PATH_SEP_CHAR , ext_job->name );
    util_make_path( ext_job->license_path );
    printf("License for %s in %s \n",ext_job->name , ext_job->license_path);
  }
}



void ext_job_set_max_time( ext_job_type * ext_job , int max_time ) {
  ext_job->max_running_minutes = max_time;
}




void ext_job_set_executable(ext_job_type * ext_job, const char * executable) {
  /**

     The portable exe can be a <...> string, i.e. not ready yet. Then
     we just have to trust the user to provide something sane in the
     end. If on the other hand executable points to an existing file
     we:

      1. Call util_alloc_realpth() to get the full absolute path.
      2. Require that it is a executable file.
      3. If current user is owner we update the access rights to a+rx.
  
  */
  if (util_file_exists( executable )) {
    char * full_path = util_alloc_realpath( executable );
    __update_mode( full_path , S_IRUSR + S_IWUSR + S_IXUSR + S_IRGRP + S_IWGRP + S_IXGRP + S_IROTH + S_IXOTH);  /* u:rwx  g:rwx  o:rx */
    
    if (util_is_executable( full_path )) 
      ext_job->executable = util_realloc_string_copy(ext_job->executable , full_path);
    else {
      fprintf(stderr , "** You do not have execute rights to:%s - job will not be available.\n" , full_path);
      ext_job->__valid = false;  /* Mark the job as NOT successfully installed - the ext_job 
                                    instance will later be freed and discarded. */
    }
    free( full_path );
  } else 
    ext_job->executable = util_realloc_string_copy(ext_job->executable , executable);
  /* We take the chance that user will supply a valid subst key for this later. */
}

/**
   Observe that this does NOT reread the ext_job instance from the new
   config_file.
*/


/*****************************************************************/
/* Scalar set and get functions                                  */

void ext_job_set_config_file(ext_job_type * ext_job, const char * config_file) {
  ext_job->config_file = util_realloc_string_copy(ext_job->config_file , config_file);
}

const char * ext_job_get_config_file(const ext_job_type * ext_job) {
  return ext_job->config_file;
}

void ext_job_set_target_file(ext_job_type * ext_job, const char * target_file) {
  ext_job->target_file = util_realloc_string_copy(ext_job->target_file , target_file);
}

const char * ext_job_get_target_file(const ext_job_type * ext_job) {
  return ext_job->target_file;
}

const char * ext_job_get_executable(const ext_job_type * ext_job) {
  return ext_job->executable;
}

void ext_job_set_start_file(ext_job_type * ext_job, const char * start_file) {
  ext_job->start_file = util_realloc_string_copy(ext_job->start_file , start_file);
}

const char * ext_job_get_start_file(const ext_job_type * ext_job) {
  return ext_job->start_file;
}

void ext_job_set_name(ext_job_type * ext_job, const char * name) {
  ext_job->name = util_realloc_string_copy(ext_job->name , name);
}

const char * ext_job_get_name(const ext_job_type * ext_job) {
  return ext_job->name;
}

void ext_job_set_lsf_request(ext_job_type * ext_job, const char * lsf_request) {
  ext_job->lsf_resources = util_realloc_string_copy(ext_job->lsf_resources , lsf_request);
}

const char * ext_job_get_lsf_request(const ext_job_type * ext_job) {
  return ext_job->lsf_resources;
}

void ext_job_set_stdin_file(ext_job_type * ext_job, const char * stdin_file) {
  ext_job->stdin_file = util_realloc_string_copy(ext_job->stdin_file , stdin_file);
}

const char * ext_job_get_stdin_file(const ext_job_type * ext_job) {
  return ext_job->stdin_file;
}

void ext_job_set_stdout_file(ext_job_type * ext_job, const char * stdout_file) {
  ext_job->stdout_file = util_realloc_string_copy(ext_job->stdout_file , stdout_file);
}

const char * ext_job_get_stdout_file(const ext_job_type * ext_job) {
  return ext_job->stdout_file;
}

void ext_job_set_stderr_file(ext_job_type * ext_job, const char * stderr_file) {
  ext_job->stderr_file = util_realloc_string_copy(ext_job->stderr_file , stderr_file);
}

const char * ext_job_get_stderr_file(const ext_job_type * ext_job) {
  return ext_job->stderr_file;
}

void ext_job_set_max_running( ext_job_type * ext_job , int max_running) {
  ext_job->max_running = max_running;
  if (max_running > 0)
    ext_job_init_license_control( ext_job );
}

int ext_job_get_max_running( const ext_job_type * ext_job ) {
  return ext_job->max_running;
}

void ext_job_set_max_running_minutes( ext_job_type * ext_job , int max_running_minutes) {
  ext_job->max_running_minutes = max_running_minutes;
}

int ext_job_get_max_running_minutes( const ext_job_type * ext_job ) {
  return ext_job->max_running_minutes;
}

/*****************************************************************/

void ext_job_set_private_arg(ext_job_type * ext_job, const char * key , const char * value) {
  subst_list_insert_copy( ext_job->private_args  , key , value , NULL);
}

void ext_job_add_environment(ext_job_type *ext_job , const char * key , const char * value) {
  hash_insert_hash_owned_ref( ext_job->environment , key , util_alloc_string_copy( value ) , free);
}


void ext_job_clear_environment( ext_job_type * ext_job ) {
  hash_clear( ext_job->environment );
}

hash_type * ext_job_get_environment( ext_job_type * ext_job ) {
  return ext_job->environment;
}


/*****************************************************************/

static void __fprintf_string(FILE * stream , const char * s , const subst_list_type * private_args, const subst_list_type * global_args) {
  char * tmp = subst_list_alloc_filtered_string( private_args , s ); /* internal filtering first */
  
  if (global_args != NULL) {
    fprintf(stream,"\"");
    subst_list_filtered_fprintf( global_args , tmp , stream );
    fprintf(stream,"\"");
  } else
    fprintf(stream,"\"%s\"" , tmp);

  free( tmp );
}

 
static void __fprintf_python_string(FILE * stream , const char * id , const char * value, const subst_list_type * private_args, const subst_list_type * global_args) {
  fprintf(stream , "\"%s\" : " , id);
  if (value == NULL)
    fprintf(stream,"None");
  else 
    __fprintf_string(stream , value , private_args , global_args);
}


static void __fprintf_python_list(FILE * stream , const char * id , const stringlist_type * list , const subst_list_type * private_args, const subst_list_type * global_args ) {
  int size;
  int i;
  fprintf(stream , "\"%s\" : " , id);
  fprintf(stream,"[");
  if (list == NULL)
    size = 0;
  else
    size = stringlist_get_size(list);

  for (i = 0; i < size; i++) {
    const char * value = stringlist_iget(list , i);
    __fprintf_string(stream , value , private_args , global_args);
    if (i < (size - 1))
      fprintf(stream,",");
  }
  fprintf(stream,"]");
}



static void __fprintf_python_hash(FILE * stream , const char * id , hash_type * hash, const subst_list_type * private_args, const subst_list_type * global_args) {
  fprintf(stream , "\"%s\" : " , id);
  int   hash_size = hash_get_size(hash);
  if (hash_size > 0) {
    int   counter   = 0;
    fprintf(stream,"{");
    hash_iter_type * iter = hash_iter_alloc(hash);
    const char * key = hash_iter_get_next_key(iter);
    while (key != NULL) {
      const char * value = hash_get(hash , key);

      fprintf(stream,"\"%s\" : " , key);
      __fprintf_string(stream , value , private_args , global_args);
      
      if (counter < (hash_size - 1))
	fprintf(stream,",");
      
      key = hash_iter_get_next_key(iter);
    }
    fprintf(stream,"}");
  } else
    fprintf(stream , "None");
}


static void __fprintf_python_int( FILE * stream , const char * key , int value) {
  if (value > 0)
    fprintf(stream , "\"%s\" : %d" , key , value);
  else
    fprintf(stream , "\"%s\" : None" , key);
}


static void __end_line(FILE * stream) {
  fprintf(stream,",\n");
}

static void __indent(FILE * stream, int indent) {
  int i;
  for (i = 0; i < indent; i++)
    fprintf(stream," ");
}




void ext_job_python_fprintf(const ext_job_type * ext_job, FILE * stream, const subst_list_type * global_args) {
  fprintf(stream," {");

  __indent(stream, 0); __fprintf_python_string(stream , "name"  	      , ext_job->name                , ext_job->private_args , NULL);        __end_line(stream);
  __indent(stream, 2); __fprintf_python_string(stream , "executable" 	      , ext_job->executable          , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_string(stream , "target_file"  	      , ext_job->target_file         , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_string(stream , "start_file"  	      , ext_job->start_file          , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_string(stream , "stdout"    	      , ext_job->stdout_file         , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_string(stream , "stderr"    	      , ext_job->stderr_file         , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_string(stream , "stdin"     	      , ext_job->stdin_file          , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_list(stream   , "argList"      	      , ext_job->argv                , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_hash(stream   , "environment"  	      , ext_job->environment         , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_string(stream , "license_path"        , ext_job->license_path        , ext_job->private_args, global_args);  __end_line(stream);
  __indent(stream, 2); __fprintf_python_int( stream   , "max_running_minutes" , ext_job->max_running_minutes );                                      __end_line(stream);
  __indent(stream, 2); __fprintf_python_int( stream   , "max_running"         , ext_job->max_running );                                              __end_line(stream);
  
  fprintf(stream,"}");
}


#define PRINT_KEY_STRING( stream , key , value ) \
if (value != NULL)                               \
{                                                \
   fprintf(stream , "%16s ", key);               \
   fprintf(stream , "%s\n" , value);             \
}
                                                                     
                                        
#define PRINT_KEY_INT( stream , key , value ) \
if (value != 0)                               \
{                                             \
   fprintf(stream , "%16s ", key);            \
   fprintf(stream , "%d\n" , value);          \
}


/**
   Observe that the job will save itself to the internalized
   config_file; if you wish to save to some other place you must call
   ext_job_set_config_file() first.
*/

void ext_job_save( const ext_job_type * ext_job ) {
  FILE * stream = util_mkdir_fopen( ext_job->config_file , "w" );
  
  PRINT_KEY_STRING( stream , "EXECUTABLE"       , ext_job->executable);
  PRINT_KEY_STRING( stream , "STDIN"            , ext_job->stdin_file);
  PRINT_KEY_STRING( stream , "STDERR"           , ext_job->stderr_file);
  PRINT_KEY_STRING( stream , "STDOUT"           , ext_job->stdout_file);
  PRINT_KEY_STRING( stream , "TARGET_FILE"      , ext_job->target_file);
  PRINT_KEY_STRING( stream , "LSF_RESOURCES"    , ext_job->lsf_resources);
  PRINT_KEY_STRING( stream , "START_FILE"       , ext_job->start_file);
  PRINT_KEY_INT( stream , "MAX_RUNNING"         , ext_job->max_running);
  PRINT_KEY_INT( stream , "MAX_RUNNING_MINUTES" , ext_job->max_running_minutes);

  if (stringlist_get_size( ext_job->argv ) > 0) {
    fprintf(stream , "%16s" , "ARGLIST");
    stringlist_fprintf( ext_job->argv , " " , stream );
    fprintf(stream , "\n");
  }
  if (hash_get_size( ext_job->environment ) > 0) {
    hash_iter_type * hash_iter = hash_iter_alloc( ext_job->environment );
    while (!hash_iter_is_complete( hash_iter )) {
      const char * key = hash_iter_get_next_key( hash_iter );
      fprintf(stream, "%16s  %16s  %s\n" , "ENV" , key , (const char *) hash_get( ext_job->environment , key ));
    }
    hash_iter_free( hash_iter );
  }
  fclose( stream );
}

#undef PRINT_KEY_STRING
#undef PRINT_KEY_INT



void ext_job_fprintf(const ext_job_type * ext_job , FILE * stream) {
  fprintf(stream , "%s(", ext_job->name);
  subst_list_fprintf(ext_job->private_args , stream);
  fprintf(stream , ")  ");
}





const char * ext_job_get_lsf_resources(const ext_job_type * ext_job) {
  return ext_job->lsf_resources;
}
 

ext_job_type * ext_job_fscanf_alloc(const char * name , const char * license_root_path , bool private_job , const char * config_file) {
  {
    mode_t target_mode = S_IRUSR + S_IWUSR + S_IRGRP + S_IWGRP + S_IROTH;  /* u+rw  g+rw  o+r */
    __update_mode( config_file , target_mode );
  }
  
  if (util_entry_readable( config_file)) {
    ext_job_type * ext_job = ext_job_alloc(name , license_root_path , private_job);
    config_type  * config  = config_alloc(  );
    
    ext_job_set_config_file( ext_job , config_file );
    {
      config_item_type * item;
      item = config_add_item(config , "MAX_RUNNING"         , false , false); config_item_set_argc_minmax(item  , 1 , 1 , (const config_item_types [1]) {CONFIG_INT});
      item = config_add_item(config , "STDIN"  	            , false , false); config_item_set_argc_minmax(item  , 1 , 1 , NULL);
      item = config_add_item(config , "STDOUT" 	            , false , false); config_item_set_argc_minmax(item  , 1 , 1 , NULL);
      item = config_add_item(config , "STDERR" 	            , false , false); config_item_set_argc_minmax(item  , 1 , 1 , NULL);
      item = config_add_item(config , "EXECUTABLE"          , false , false); config_item_set_argc_minmax(item  , 1 , 1 , NULL);
      item = config_add_item(config , "TARGET_FILE"         , false , false); config_item_set_argc_minmax(item  , 1 , 1 , NULL);
      item = config_add_item(config , "START_FILE"          , false , false); config_item_set_argc_minmax(item  , 1 , 1 , NULL);
      item = config_add_item(config , "ENV"                 , false , true ); config_item_set_argc_minmax(item  , 2 , 2 , NULL);
      item = config_add_item(config , "ARGLIST"             , false , true ); config_item_set_argc_minmax(item  , 1 ,-1 , NULL);
      item = config_add_item(config , "LSF_RESOURCES"       , false , false); config_item_set_argc_minmax(item  , 1 ,-1 , NULL);
      item = config_add_item(config , "MAX_RUNNING_MINUTES" , false , false); config_item_set_argc_minmax(item  , 1 , 1 , (const config_item_types [1]) {CONFIG_INT});
    }
    config_add_alias(config , "EXECUTABLE" , "PORTABLE_EXE");
    config_parse(config , config_file , "--" , NULL , NULL , NULL , false , true);
    {
      if (config_item_set(config , "STDIN"))  	             ext_job_set_stdin_file(ext_job       , config_iget(config  , "STDIN" , 0,0));
      if (config_item_set(config , "STDOUT")) 	             ext_job_set_stdout_file(ext_job      , config_iget(config  , "STDOUT" , 0,0));
      if (config_item_set(config , "STDERR")) 	             ext_job_set_stderr_file(ext_job      , config_iget(config  , "STDERR" , 0,0));
      if (config_item_set(config , "TARGET_FILE"))           ext_job_set_target_file(ext_job      , config_iget(config  , "TARGET_FILE" , 0,0));
      if (config_item_set(config , "START_FILE"))            ext_job_set_start_file(ext_job       , config_iget(config  , "START_FILE" , 0,0));
      if (config_item_set(config , "EXECUTABLE"))            ext_job_set_executable(ext_job       , config_iget(config  , "EXECUTABLE" , 0,0));
      if (config_item_set(config , "MAX_RUNNING"))           ext_job_set_max_running(ext_job      , config_iget_as_int(config  , "MAX_RUNNING" , 0,0));
      if (config_item_set(config , "MAX_RUNNING_MINUTES"))   ext_job_set_max_time(ext_job         , config_iget_as_int(config  , "MAX_RUNNING_MINUTES" , 0,0));
 
      if (config_item_set(config , "LSF_RESOURCES")) {
        char * lsf_resources = stringlist_alloc_joined_string(config_get_stringlist_ref(config , "LSF_RESOURCES") , " ");
        ext_job_set_lsf_request(ext_job   , lsf_resources);
        free(lsf_resources);
      }

      if (config_item_set(config , "ARGLIST")) 
        ext_job->argv = config_alloc_complete_stringlist(config , "ARGLIST");
        
      /**
         The code assumes that the hash tables are valid, can not be NULL:
      */
      if (config_item_set(config , "ENV")) 
        ext_job->environment = config_alloc_hash(config , "ENV");

    }
    config_free(config);
    
    if (!ext_job->__valid) {
      /* 
         Something NOT OK (i.e. EXECUTABLE now); free the job instance and return NULL:
      */
      ext_job_free( ext_job );
      ext_job = NULL;
      fprintf(stderr,"** Warning: job: \'%s\' not available ... \n", name );
    }
    
    return ext_job;
  } else {
    fprintf(stderr,"** Warning: you do not have permission to read file:\'%s\' - job:%s not available. \n", config_file , name);
    return NULL;
  }
}
 
 
const stringlist_type * ext_job_get_arglist( const ext_job_type * ext_job ) {
  return ext_job->argv;
}


/**
   
*/

const char * ext_job_get_arglist_as_string( ext_job_type * ext_job ) {
  if (stringlist_get_size( ext_job->argv ) == 0)
    return NULL;
  else {
    const char * sep = "  ";
    int argc =  stringlist_get_size( ext_job->argv );
    int i;
    buffer_type * buffer = buffer_alloc( 512 );
    for (i = 0; i < argc; i++) {
      const char * arg = stringlist_iget( ext_job->argv , i );
      bool quote       = false;
      if (strchr(arg , ' ') != NULL)
        quote = true;

      if (quote)
        buffer_fwrite_char( buffer , ' ' );
      buffer_fwrite_char_ptr( buffer , arg );
      if (quote)
        buffer_fwrite_char( buffer , ' ' );
      
      if (i < (argc - 1))
        buffer_fwrite_char_ptr( buffer , sep );
    }
    buffer_fwrite_char( buffer , '\0');
    util_safe_free(ext_job->argv_string);
    ext_job->argv_string = buffer_alloc_data_copy( buffer );
    buffer_free( buffer );
    
    return ext_job->argv_string;
  }
}
 

void ext_job_set_arglist_from_string( ext_job_type * ext_job , const char * argv_string ) {
  parser_type * parser = parser_alloc(" " , "\"" , NULL , NULL , NULL , NULL );
  stringlist_free( ext_job->argv );
  ext_job->argv = parser_tokenize_buffer( parser , argv_string , true );
  parser_free( parser );
}


bool ext_job_is_shared( const ext_job_type * ext_job ) {
  return !ext_job->private_job;
}

bool ext_job_is_private( const ext_job_type * ext_job ) {
  return ext_job->private_job;
}


#undef ASSERT_TOKENS
