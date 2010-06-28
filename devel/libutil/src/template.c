#include <stdlib.h>
#include <stdbool.h>
#include <subst_list.h>
#include <subst_func.h>
#include <template.h>



#define TEMPLATE_TYPE_ID 7781045

struct template_struct {
  UTIL_TYPE_ID_DECLARATION;
  char            * template_file;           /* The template file - if internalize_template == false this filename can contain keys which will be replaced at instantiation time. */
  char            * template_buffer;         /* The content of the template buffer; only has valid content if internalize_template == true. */
  bool              internalize_template;    /* Should the template be loadad and internalized at template_alloc(). */
  subst_list_type * arg_list;                /* Key-value mapping established at alloc time. */
  char            * arg_string;              /* A string representation of the arguments - ONLY used for a _get_ function. */ 
};




/**
   Iff the template is set up with internaliz_template == false the
   template content is loaded at instantiation time, and in that case
   the name of the template file can contain substitution characters -
   i.e. in this case different instance can use different source
   templates.

   To avoid race issues this function does not set actually update the
   state of the template object.
*/

static char * template_load( const template_type * template , const subst_list_type * ext_arg_list) {
  int buffer_size;
  char * template_file = util_alloc_string_copy( template->template_file );
  char * template_buffer;
  
  subst_list_update_string( template->arg_list , &template_file);
  if (ext_arg_list != NULL)
    subst_list_update_string( ext_arg_list , &template_file);
  
  template_buffer = util_fread_alloc_file_content( template_file , &buffer_size );
  free( template_file );
  
  return template_buffer;
}



void template_set_template_file( template_type * template , const char * template_file) {
  template->template_file = util_realloc_string_copy( template->template_file , template_file );
  if (template->internalize_template) {
    util_safe_free( template->template_buffer );
    template->template_buffer = template_load( template , NULL );  
  }
}

/** This will not instantiate */
const char * template_get_template_file( const template_type * template ) {
  return template->template_file; 
}



/**
   This function allocates a template object based on the source file
   'template_file'. If @internalize_template is true the template
   content will be read and internalized at boot time, otherwise that
   is deferred to template instantiation time (in which case the
   template file can change dynamically).
*/


template_type * template_alloc( const char * template_file , bool internalize_template , subst_list_type * parent_subst) {
  template_type * template = util_malloc( sizeof * template , __func__);
  UTIL_TYPE_ID_INIT(template , TEMPLATE_TYPE_ID);
  template->arg_list             = subst_list_alloc( parent_subst );
  template->template_buffer      = NULL;
  template->template_file        = NULL;
  template->internalize_template = internalize_template;
  template->arg_string           = NULL;
  
  template_set_template_file( template , template_file );
  return template;
}



void template_free( template_type * template ) {
  subst_list_free( template->arg_list );
  util_safe_free( template->template_file );
  util_safe_free( template->template_buffer );
  util_safe_free( template->arg_string );
  free( template );
}



/**
   This function will create the file @__target_file based on the
   template instance. Before the target file is written all the
   internal substitutions and then subsequently the subsititutions in
   @arg_list will be performed. The input @arg_list can be NULL - in
   which case this is more like copy operation.

   Observe that:
   
    1. Substitutions will be performed on @__target_file

    2. @__target_file can contain path components.

    3. If internalize_template == false subsititions will be performed
       on the filename of the file with template content.
  
    4. If the parameter @override_symlink is true the function will
       have the following behaviour:

         If the target_file already exists as a symbolic link, the
         symbolic link will be removed prior to creating the instance,
         ensuring that a remote file is not updated.

*/
   


void template_instansiate( const template_type * template , const char * __target_file , const subst_list_type * arg_list , bool override_symlink) {
  char * target_file = util_alloc_string_copy( __target_file );

  /* Finding the name of the target file. */
  subst_list_update_string( template->arg_list , &target_file);
  if (arg_list != NULL) subst_list_update_string( arg_list , &target_file );

  {
    char * buffer;
    /* Loading the template - possibly expanding keys in the filename */
    if (template->internalize_template)
      buffer = util_alloc_string_copy( template->template_buffer);
    else
      buffer = template_load( template , arg_list );
    
    /* Substitutions on the content. */
    subst_list_update_string( template->arg_list , &buffer );
    if (arg_list != NULL) subst_list_update_string( arg_list , &buffer );

    /* 
       Check if target file already exists as a symlink, 
       and remove it if override_symlink is true. 
    */
    if (override_symlink) {
      if (util_is_link( target_file ))
        unlink( target_file );
    }
    
    /* Write the content out. */
    {
      FILE * stream = util_mkdir_fopen( target_file , "w");
      fprintf(stream , "%s" , buffer);
      fclose( stream );
    }
    free( buffer );
  }
  
  free( target_file );
}


/**
   Add an internal key_value pair. This substitution will be performed
   before the internal substitutions.
*/
void template_add_arg( template_type * template , const char * key , const char * value ) {
  subst_list_append_copy( template->arg_list , key , value , NULL /* No doc_string */);
}


void template_clear_args( template_type * template ) {
  subst_list_clear( template->arg_list );
}


int template_add_args_from_string( template_type * template , const char * arg_string) {
  return subst_list_add_from_string( template->arg_list , arg_string , true);
}


char * template_get_args_as_string( template_type * template ) {
  util_safe_free( template->arg_string );
  template->arg_string = subst_list_alloc_string_representation( template->arg_list );
  return template->arg_string;
}
