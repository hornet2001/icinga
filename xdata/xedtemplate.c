/*****************************************************************************
 *
 * XEDTEMPLATE.C - Template-based extended information data input routines
 *
 * Copyright (c) 2001-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   01-04-2003
 *
 * Description:
 *
 *    1) Read
 *    2) Resolve
 *    3) Replicate
 *    4) Register
 *    5) Cleanup
 *
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/


/*********** COMMON HEADER FILES ***********/

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"
#include "../cgi/edata.h"


/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xedtemplate.h"


extern hostgroup *hostgroup_list;


xedtemplate_hostextinfo *xedtemplate_hostextinfo_list=NULL;
xedtemplate_serviceextinfo *xedtemplate_serviceextinfo_list=NULL;

int xedtemplate_current_object_type=XEDTEMPLATE_NONE;



/******************************************************************/
/************* TOP-LEVEL CONFIG DATA INPUT FUNCTION ***************/
/******************************************************************/

/* process all config files - CGIs pass in name of CGI config file */
int xedtemplate_read_extended_object_config_data(char *cgi_config_file, int options){
	char config_file[MAX_FILENAME_LENGTH];
	char input[MAX_XEDTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fpm;
	int result=OK;

#ifdef DEBUG0
	printf("xedtemplate_read_extended_object_config_data() start\n");
#endif

	/* open the CGI config file for reading (we need to find all the config files to read) */
	fpm=fopen(cgi_config_file,"r");
	if(fpm==NULL)
		return ERROR;

	/* initialize object definition lists to NULL - CGIs core dump w/o this (why?) */
	xedtemplate_hostextinfo_list=NULL;
	xedtemplate_serviceextinfo_list=NULL;

	/* read in all lines from the main config file */
	for(fgets(input,sizeof(input)-1,fpm);!feof(fpm);fgets(input,sizeof(input)-1,fpm)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]==';' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		/* strip input */
		xedtemplate_strip(input);

		temp_ptr=strtok(input,"=");
		if(temp_ptr==NULL)
			continue;

		/* process a single config file */
		if(!strcmp(temp_ptr,"xedtemplate_config_file")){

			/* get the config file name */
			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(config_file,temp_ptr,sizeof(config_file)-1);
			config_file[sizeof(config_file)-1]='\x0';

			/* process the config file... */
			result=xedtemplate_process_config_file(config_file,options);

			/* DON'T BAIL OUT ON ERRORS */
		        }

		/* process all files in a config directory */
		else if(!strcmp(temp_ptr,"xedtemplate_config_dir")){

			/* get the config directory name */
			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(config_file,temp_ptr,sizeof(config_file)-1);
			config_file[sizeof(config_file)-1]='\x0';

			/* strip trailing / if necessary */
			if(config_file[strlen(config_file)-1]=='/')
				config_file[strlen(config_file)-1]='\x0';

			/* process the config directory... */
			result=xedtemplate_process_config_dir(config_file,options);

			/* DON'T BAIL OUT ON ERRORS */
		        }
	        }

	fclose(fpm);

	/* resolve objects definitions */
	result=xedtemplate_resolve_objects();

	/* duplicate object definitions */
	result=xedtemplate_duplicate_objects();

	/* register objects */
	result=xedtemplate_register_objects();

	/* cleanup */
	xedtemplate_free_memory();

#ifdef DEBUG0
	printf("xedtemplate_read_extended_object_config_data() end\n");
#endif

	return result;
	}




/* process data in a specific config file */
int xedtemplate_process_config_file(char *filename, int options){
	FILE *fp;
	char input[MAX_XEDTEMPLATE_INPUT_BUFFER];
	int in_definition=FALSE;
	char *temp_ptr;
	int current_line=0;
	int result=OK;
	register int x;
	register int y;

#ifdef DEBUG0
	printf("xedtemplate_process_config_file() start\n");
#endif

	/* open the config file for reading */
	fp=fopen(filename,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the config file */
	for(fgets(input,sizeof(input)-1,fp);!feof(fp);fgets(input,sizeof(input)-1,fp)){

		current_line++;

		/* skip empty lines */
		if(input[0]=='#' || input[0]==';' || input[0]=='\r' || input[0]=='\n')
			continue;

		/* grab data before comment delimiter - faster than a strtok() and strncpy()... */
		for(x=0;input[x]!='\x0';x++)
			if(input[x]==';')
				break;
		input[x]='\x0';

		/* strip input */
		xedtemplate_strip(input);

		/* skip blank lines */
		if(input[0]=='\x0')
			continue;

		/* this is the start of an object definition */
		else if(strstr(input,"define")==input){

			/* get the type of object we're defining... */
			for(x=6;input[x]!='\x0';x++)
				if(input[x]!=' ' && input[x]!='\t')
					break;
			for(y=0;input[x]!='\x0';x++){
				if(input[x]==' ' || input[x]=='\t' ||  input[x]=='{')
					break;
				else
					input[y++]=input[x];
			        }
			input[y]='\x0';

			/* make sure an object type is specified... */
			if(strcmp(input,"hostextinfo") && strcmp(input,"serviceextinfo"))
				continue;

			/* we're already in an object definition... */
			if(in_definition==TRUE)
				continue;

			/* start a new definition */
			xedtemplate_begin_object_definition(input);

			in_definition=TRUE;
		        }

		/* this is the close of an object definition */
		else if(!strcmp(input,"}") && in_definition==TRUE){

			in_definition=FALSE;

			/* close out current definition */
			xedtemplate_end_object_definition();
		        }

		/* this is a directive inside an object definition */
		else if(in_definition==TRUE){
			
			/* add directive to object definition */
			xedtemplate_add_object_property(input);
		        }
	        }

	/* close file */
	fclose(fp);

	/* whoops - EOF while we were in the middle of an object definition... */
	if(in_definition==TRUE && result==OK){
		result=ERROR;
	        }

#ifdef DEBUG0
	printf("xedtemplate_process_config_file() end\n");
#endif

	return result;
        }



/* process all files in a specific config directory */
int xedtemplate_process_config_dir(char *dirname, int options){
	char config_file[MAX_FILENAME_LENGTH];
	DIR *dirp;
	struct dirent *dirfile;
	int result=OK;
	int x;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xedtemplate_process_config_dir() start\n");
#endif

	/* open the directory for reading */
	dirp=opendir(dirname);
        if(dirp==NULL)
		result=ERROR;

	/* process all files/subdirectories in the directory... */
	while((dirfile=readdir(dirp))!=NULL){

		/* test if this is a config file... */
		x=strlen(dirfile->d_name);
		if(x>4 && !strcmp(dirfile->d_name+(x-4),".cfg")){

#ifdef _DIRENT_HAVE_D_TYPE
			/* only process normal files */
			if(dirfile->d_type!=DT_REG)
				continue;
#endif
			/* create the full path to the config file */
			snprintf(config_file,sizeof(config_file)-1,"%s/%s",dirname,dirfile->d_name);
			config_file[sizeof(config_file)-1]='\x0';

			/* process the config file */
			result=xedtemplate_process_config_file(config_file,options);

			/* DON'T BAIL OUT ON ERROR */
		        }

#ifdef _DIRENT_HAVE_D_TYPE
		/* recurse into subdirectories... */
		if(dirfile->d_type==DT_DIR){

			/* ignore current, parent and hidden directory entries */
			if(dirfile->d_name[0]=='.')
				continue;

			/* create the full path to the config directory */
			snprintf(config_file,sizeof(config_file)-1,"%s/%s",dirname,dirfile->d_name);
			config_file[sizeof(config_file)-1]='\x0';

			/* process the config directory */
			result=xedtemplate_process_config_dir(config_file,options);

			/* DON'T BAIL OUT ON ERROR */
		        }
#endif
		}

	closedir(dirp);

#ifdef DEBUG0
	printf("xedtemplate_process_config_dir() end\n");
#endif

	return result;
        }



/* strip newline, carriage return, and tab characters from beginning and end of a string */
void xedtemplate_strip(char *buffer){
	register int x;
	register int y;
	char ch;
	register int a;
	register int b;

	if(buffer==NULL || buffer[0]=='\x0')
		return;

	/* strip end of string */
	x=(int)strlen(buffer)-1;
	for(;x>=0;x--){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			buffer[x]='\x0';
		else
			break;
	        }

	/* reverse string */
	x=(int)strlen(buffer);
	if(x==0)
		return;
	for(a=0,b=x-1;a<b;a++,b--){
		ch=buffer[a];
		buffer[a]=buffer[b];
		buffer[b]=ch;
	        }

	/* strip beginning of string (now end of string) */
	x--;
	for(;x>=0;x--){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			buffer[x]='\x0';
		else
			break;
	        }

	/* reverse string again */
	x=(int)strlen(buffer);
	if(x==0)
		return;
	for(a=0,b=x-1;a<b;a++,b--){
		ch=buffer[a];
		buffer[a]=buffer[b];
		buffer[b]=ch;
	        }

	return;
	}





/******************************************************************/
/***************** OBJECT DEFINITION FUNCTIONS ********************/
/******************************************************************/

/* starts a new object definition */
int xedtemplate_begin_object_definition(char *input){
	int result=OK;
	xedtemplate_hostextinfo *new_hostextinfo;
	xedtemplate_serviceextinfo *new_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_begin_object_definition() start\n");
#endif

	if(!strcmp(input,"hostextinfo"))
		xedtemplate_current_object_type=XEDTEMPLATE_HOSTEXTINFO;
	else if(!strcmp(input,"serviceextinfo"))
		xedtemplate_current_object_type=XEDTEMPLATE_SERVICEEXTINFO;
	else
		return ERROR;


	/* add a new (blank) object */
	switch(xedtemplate_current_object_type){

	case XEDTEMPLATE_HOSTEXTINFO:

		/* allocate memory */
		new_hostextinfo=(xedtemplate_hostextinfo *)malloc(sizeof(xedtemplate_hostextinfo));
		if(new_hostextinfo==NULL)
			return ERROR;

		new_hostextinfo->template=NULL;
		new_hostextinfo->name=NULL;
		new_hostextinfo->host_name=NULL;
		new_hostextinfo->hostgroup_name=NULL;
		new_hostextinfo->notes_url=NULL;
		new_hostextinfo->icon_image=NULL;
		new_hostextinfo->icon_image_alt=NULL;
		new_hostextinfo->vrml_image=NULL;
		new_hostextinfo->gd2_image=NULL;
		new_hostextinfo->x_2d=-1;
		new_hostextinfo->y_2d=-1;
		new_hostextinfo->x_3d=0.0;
		new_hostextinfo->y_3d=0.0;
		new_hostextinfo->z_3d=0.0;
		new_hostextinfo->have_2d_coords=FALSE;
		new_hostextinfo->have_3d_coords=FALSE;

		new_hostextinfo->has_been_resolved=FALSE;
		new_hostextinfo->register_object=TRUE;

		/* add new timeperiod to head of list in memory */
		new_hostextinfo->next=xedtemplate_hostextinfo_list;
		xedtemplate_hostextinfo_list=new_hostextinfo;

		break;

	case XEDTEMPLATE_SERVICEEXTINFO:

		/* allocate memory */
		new_serviceextinfo=(xedtemplate_serviceextinfo *)malloc(sizeof(xedtemplate_serviceextinfo));
		if(new_serviceextinfo==NULL)
			return ERROR;

		new_serviceextinfo->template=NULL;
		new_serviceextinfo->name=NULL;
		new_serviceextinfo->host_name=NULL;
		new_serviceextinfo->hostgroup_name=NULL;
		new_serviceextinfo->service_description=NULL;
		new_serviceextinfo->notes_url=NULL;
		new_serviceextinfo->icon_image=NULL;
		new_serviceextinfo->icon_image_alt=NULL;

		new_serviceextinfo->has_been_resolved=FALSE;
		new_serviceextinfo->register_object=TRUE;

		/* add new timeperiod to head of list in memory */
		new_serviceextinfo->next=xedtemplate_serviceextinfo_list;
		xedtemplate_serviceextinfo_list=new_serviceextinfo;

		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("xedtemplate_begin_object_definition() end\n");
#endif

	return result;
        }



/* adds a property to an object definition */
int xedtemplate_add_object_property(char *input){
	int result=OK;
	char variable[MAX_XEDTEMPLATE_INPUT_BUFFER];
	char value[MAX_XEDTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	xedtemplate_hostextinfo *temp_hostextinfo;
	xedtemplate_serviceextinfo *temp_serviceextinfo;
	register int x;
	register int y;

#ifdef DEBUG0
	printf("xedtemplate_add_object_property() start\n");
#endif

	/* truncate if necessary */
	if(strlen(input)>MAX_XEDTEMPLATE_INPUT_BUFFER)
		input[MAX_XEDTEMPLATE_INPUT_BUFFER-1]='\x0';

	/* get variable name */
	for(x=0,y=0;input[x]!='\x0';x++){
		if(input[x]==' ' || input[x]=='\t')
			break;
		else
			variable[y++]=input[x];
	        }
	variable[y]='\x0';
			
	/* get variable value */
	if(x>=strlen(input))
		return ERROR;
	for(y=0;input[x]!='\x0';x++)
		value[y++]=input[x];
	value[y]='\x0';

	/*
	printf("RAW VARIABLE: '%s'\n",variable);
	printf("RAW VALUE: '%s'\n",value);
	*/

#ifdef RUN_SLOW_AS_HELL
	xedtemplate_strip(variable);
#endif
	xedtemplate_strip(value);

	/*
	printf("STRIPPED VARIABLE: '%s'\n",variable);
	printf("STRIPPED VALUE: '%s'\n\n",value);
	*/

	switch(xedtemplate_current_object_type){

	case XEDTEMPLATE_HOSTEXTINFO:
		
		temp_hostextinfo=xedtemplate_hostextinfo_list;

		if(!strcmp(variable,"use")){
			temp_hostextinfo->template=strdup(value);
			if(temp_hostextinfo->template==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"name")){
			temp_hostextinfo->name=strdup(value);
			if(temp_hostextinfo->name==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"host_name")){
			temp_hostextinfo->host_name=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->host_name==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->host_name,value);
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroup_name")){
			temp_hostextinfo->hostgroup_name=strdup(value);
			if(temp_hostextinfo->hostgroup_name==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"notes_url")){
			temp_hostextinfo->notes_url=strdup(value);
			if(temp_hostextinfo->notes_url==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"icon_image")){
			temp_hostextinfo->icon_image=strdup(value);
			if(temp_hostextinfo->icon_image==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"icon_image_alt")){
			temp_hostextinfo->icon_image_alt=strdup(value);
			if(temp_hostextinfo->icon_image_alt==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"vrml_image")){
			temp_hostextinfo->vrml_image=strdup(value);
			if(temp_hostextinfo->vrml_image==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"gd2_image")|| !strcmp(variable,"statusmap_image")){
			temp_hostextinfo->gd2_image=strdup(value);
			if(temp_hostextinfo->gd2_image==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"2d_coords")){
			temp_ptr=strtok(value,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->x_2d=atoi(temp_ptr);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->y_2d=atoi(temp_ptr);
			temp_hostextinfo->have_2d_coords=TRUE;
		        }
		else if(!strcmp(variable,"3d_coords")){
			temp_ptr=strtok(value,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->x_3d=strtod(temp_ptr,NULL);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->y_3d=strtod(temp_ptr,NULL);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->z_3d=strtod(temp_ptr,NULL);
			temp_hostextinfo->have_3d_coords=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_hostextinfo->register_object=(atoi(value)>0)?TRUE:FALSE;

		break;
	
	case XEDTEMPLATE_SERVICEEXTINFO:
		
		temp_serviceextinfo=xedtemplate_serviceextinfo_list;

		if(!strcmp(variable,"use")){
			temp_serviceextinfo->template=strdup(value);
			if(temp_serviceextinfo->template==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"name")){
			temp_serviceextinfo->name=strdup(value);
			if(temp_serviceextinfo->name==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"host_name")){
			temp_serviceextinfo->host_name=strdup(value);
			if(temp_serviceextinfo->host_name==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroup_name")){
			temp_serviceextinfo->hostgroup_name=strdup(value);
			if(temp_serviceextinfo->hostgroup_name==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"service_description")){
			temp_serviceextinfo->service_description=strdup(value);
			if(temp_serviceextinfo->service_description==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"notes_url")){
			temp_serviceextinfo->notes_url=strdup(value);
			if(temp_serviceextinfo->notes_url==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"icon_image")){
			temp_serviceextinfo->icon_image=strdup(value);
			if(temp_serviceextinfo->icon_image==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"icon_image_alt")){
			temp_serviceextinfo->icon_image_alt=strdup(value);
			if(temp_serviceextinfo->icon_image_alt==NULL)
				return ERROR;
		        }
		else if(!strcmp(variable,"register"))
			temp_serviceextinfo->register_object=(atoi(value)>0)?TRUE:FALSE;

		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("xedtemplate_add_object_property() end\n");
#endif

	return result;
        }



/* completes an object definition */
int xedtemplate_end_object_definition(void){

#ifdef DEBUG0
	printf("xedtemplate_end_object_definition() start\n");
#endif

	xedtemplate_current_object_type=XEDTEMPLATE_NONE;

#ifdef DEBUG0
	printf("xedtemplate_end_object_definition() end\n");
#endif

	return OK;
        }





/******************************************************************/
/***************** OBJECT DUPLICATION FUNCTIONS *******************/
/******************************************************************/


/* duplicates object definitions */
int xedtemplate_duplicate_objects(void){
	xedtemplate_hostextinfo *temp_hostextinfo;
	xedtemplate_serviceextinfo *temp_serviceextinfo;
	xedtemplate_hostlist *temp_hostlist;
	xedtemplate_hostlist *this_hostlist;
	int first_item;
	int result=OK;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_objects() start\n");
#endif


	/****** DUPLICATE HOSTEXTINFO DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){

		/* skip definitions without enough data */
		if(temp_hostextinfo->hostgroup_name==NULL && temp_hostextinfo->host_name==NULL)
			continue;

		/* get list of hosts */
		temp_hostlist=xedtemplate_expand_hostgroups_and_hosts(temp_hostextinfo->hostgroup_name,temp_hostextinfo->host_name);
		if(temp_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand hostgroups and/or hosts specified in extended host info (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_service->_config_file),temp_service->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* add a copy of the definition for every host in the hostgroup/host name list */
		first_item=TRUE;
		for(this_hostlist=temp_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

			/* if this is the first duplication, use the existing entry */
			if(first_item==TRUE){

				free(temp_hostextinfo->host_name);
				temp_hostextinfo->host_name=strdup(this_hostlist->host_name);
				if(temp_hostextinfo->host_name==NULL){
					xedtemplate_free_hostlist(temp_hostlist);
					return ERROR;
				        }
				first_item=FALSE;
				continue;
			        }

			/* duplicate hostextinfo definition */
			result=xedtemplate_duplicate_hostextinfo(temp_hostextinfo,this_hostlist->host_name);

			/* exit on error */
			if(result==ERROR){
				xedtemplate_free_hostlist(temp_hostlist);
				return ERROR;
			        }
		        }

		/* free memory we used for host list */
		xedtemplate_free_hostlist(temp_hostlist);
	        }


	/****** DUPLICATE SERVICEEXTINFO DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){

		/* skip definitions without enough data */
		if(temp_serviceextinfo->hostgroup_name==NULL && temp_serviceextinfo->host_name==NULL)
			continue;

		/* get list of hosts */
		temp_hostlist=xedtemplate_expand_hostgroups_and_hosts(temp_serviceextinfo->hostgroup_name,temp_serviceextinfo->host_name);
		if(temp_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand hostgroups and/or hosts specified in extended service info (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_service->_config_file),temp_service->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* add a copy of the definition for every host in the hostgroup/host name list */
		first_item=TRUE;
		for(this_hostlist=temp_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

			/* existing definition gets first host name */
			if(first_item==TRUE){
				free(temp_serviceextinfo->host_name);
				temp_serviceextinfo->host_name=strdup(this_hostlist->host_name);
				if(temp_serviceextinfo->host_name==NULL){
					xedtemplate_free_hostlist(temp_hostlist);
					return ERROR;
				        }
				first_item=FALSE;
				continue;
			        }

			/* duplicate serviceextinfo definition */
			result=xedtemplate_duplicate_serviceextinfo(temp_serviceextinfo,this_hostlist->host_name);

			/* exit on error */
			if(result==ERROR){
				xedtemplate_free_hostlist(temp_hostlist);
				return ERROR;
			        }
		        }

		/* free memory we used for host list */
		xedtemplate_free_hostlist(temp_hostlist);
	        }


#ifdef DEBUG0
	printf("xedtemplate_duplicate_objects() end\n");
#endif

	return OK;
        }



/* duplicates a hostextinfo object definition */
int xedtemplate_duplicate_hostextinfo(xedtemplate_hostextinfo *this_hostextinfo, char *host_name){
	xedtemplate_hostextinfo *new_hostextinfo;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_hostextinfo() start\n");
#endif

	new_hostextinfo=(xedtemplate_hostextinfo *)malloc(sizeof(xedtemplate_hostextinfo));
	if(new_hostextinfo==NULL)
		return ERROR;

	new_hostextinfo->template=NULL;
	new_hostextinfo->name=NULL;
	new_hostextinfo->host_name=NULL;
	new_hostextinfo->hostgroup_name=NULL;
	new_hostextinfo->notes_url=NULL;
	new_hostextinfo->icon_image=NULL;
	new_hostextinfo->icon_image_alt=NULL;
	new_hostextinfo->vrml_image=NULL;
	new_hostextinfo->gd2_image=NULL;

	/* duplicate strings (host_name member is passed in) */
	if(host_name!=NULL)
		new_hostextinfo->host_name=strdup(host_name);
	if(this_hostextinfo->template!=NULL)
		new_hostextinfo->template=strdup(this_hostextinfo->template);
	if(this_hostextinfo->name!=NULL)
		new_hostextinfo->name=strdup(this_hostextinfo->name);
	if(this_hostextinfo->notes_url!=NULL)
		new_hostextinfo->notes_url=strdup(this_hostextinfo->notes_url);
	if(this_hostextinfo->icon_image!=NULL)
		new_hostextinfo->icon_image=strdup(this_hostextinfo->icon_image);
	if(this_hostextinfo->icon_image_alt!=NULL)
		new_hostextinfo->icon_image_alt=strdup(this_hostextinfo->icon_image_alt);
	if(this_hostextinfo->vrml_image!=NULL)
		new_hostextinfo->vrml_image=strdup(this_hostextinfo->vrml_image);
	if(this_hostextinfo->gd2_image!=NULL)
		new_hostextinfo->gd2_image=strdup(this_hostextinfo->gd2_image);

	/* duplicate non-string members */
	new_hostextinfo->x_2d=this_hostextinfo->x_2d;
	new_hostextinfo->y_2d=this_hostextinfo->y_2d;
	new_hostextinfo->have_2d_coords=this_hostextinfo->have_2d_coords;
	new_hostextinfo->x_3d=this_hostextinfo->x_3d;
	new_hostextinfo->y_3d=this_hostextinfo->y_3d;
	new_hostextinfo->z_3d=this_hostextinfo->z_3d;
	new_hostextinfo->have_3d_coords=this_hostextinfo->have_3d_coords;

	new_hostextinfo->has_been_resolved=this_hostextinfo->has_been_resolved;
	new_hostextinfo->register_object=this_hostextinfo->register_object;

	/* add new object to head of list */
	new_hostextinfo->next=xedtemplate_hostextinfo_list;
	xedtemplate_hostextinfo_list=new_hostextinfo;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_hostextinfo() end\n");
#endif

	return;
        }



/* duplicates a serviceextinfo object definition */
int xedtemplate_duplicate_serviceextinfo(xedtemplate_serviceextinfo *this_serviceextinfo, char *host_name){
	xedtemplate_serviceextinfo *new_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_serviceextinfo() start\n");
#endif

	new_serviceextinfo=(xedtemplate_serviceextinfo *)malloc(sizeof(xedtemplate_serviceextinfo));
	if(new_serviceextinfo==NULL)
		return ERROR;

	new_serviceextinfo->template=NULL;
	new_serviceextinfo->name=NULL;
	new_serviceextinfo->host_name=NULL;
	new_serviceextinfo->hostgroup_name=NULL;
	new_serviceextinfo->notes_url=NULL;
	new_serviceextinfo->icon_image=NULL;
	new_serviceextinfo->icon_image_alt=NULL;

	new_serviceextinfo->has_been_resolved=this_serviceextinfo->has_been_resolved;
	new_serviceextinfo->register_object=this_serviceextinfo->register_object;

	/* duplicate strings (host_name member is passed in) */
	if(host_name!=NULL)
		new_serviceextinfo->host_name=strdup(host_name);
	if(this_serviceextinfo->template!=NULL)
		new_serviceextinfo->template=strdup(this_serviceextinfo->template);
	if(this_serviceextinfo->name!=NULL)
		new_serviceextinfo->name=strdup(this_serviceextinfo->name);
	if(this_serviceextinfo->service_description!=NULL)
		new_serviceextinfo->service_description=strdup(this_serviceextinfo->service_description);
	if(this_serviceextinfo->notes_url!=NULL)
		new_serviceextinfo->notes_url=strdup(this_serviceextinfo->notes_url);
	if(this_serviceextinfo->icon_image!=NULL)
		new_serviceextinfo->icon_image=strdup(this_serviceextinfo->icon_image);
	if(this_serviceextinfo->icon_image_alt!=NULL)
		new_serviceextinfo->icon_image_alt=strdup(this_serviceextinfo->icon_image_alt);

	/* add new object to head of list */
	new_serviceextinfo->next=xedtemplate_serviceextinfo_list;
	xedtemplate_serviceextinfo_list=new_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_serviceextinfo() end\n");
#endif

	return;
        }



/******************************************************************/
/***************** OBJECT RESOLUTION FUNCTIONS ********************/
/******************************************************************/

/* resolves object definitions */
int xedtemplate_resolve_objects(void){
	xedtemplate_hostextinfo *temp_hostextinfo;
	xedtemplate_serviceextinfo *temp_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_resolve_objects() start\n");
#endif

	/* resolve all hostextinfo objects */
	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if(xedtemplate_resolve_hostextinfo(temp_hostextinfo)==ERROR)
			return ERROR;
	        }

	/* resolve all serviceextinfo objects */
	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
		if(xedtemplate_resolve_serviceextinfo(temp_serviceextinfo)==ERROR)
			return ERROR;
	        }

#ifdef DEBUG0
	printf("xedtemplate_resolve_objects() end\n");
#endif

	return OK;
        }



/* resolves a hostextinfo object */
int xedtemplate_resolve_hostextinfo(xedtemplate_hostextinfo *this_hostextinfo){
	xedtemplate_hostextinfo *template_hostextinfo;

#ifdef DEBUG0
	printf("xedtemplate_resolve_hostextinfo() start\n");
#endif

	/* return if this object has already been resolved */
	if(this_hostextinfo->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostextinfo->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostextinfo->template==NULL)
		return OK;

	template_hostextinfo=xedtemplate_find_hostextinfo(this_hostextinfo->template);
	if(template_hostextinfo==NULL)
		return ERROR;

	/* resolve the template hostextinfo... */
	xedtemplate_resolve_hostextinfo(template_hostextinfo);

	/* apply missing properties from template hostextinfo... */
	if(this_hostextinfo->name==NULL && template_hostextinfo->name!=NULL)
		this_hostextinfo->name=strdup(template_hostextinfo->name);
	if(this_hostextinfo->host_name==NULL && template_hostextinfo->host_name!=NULL)
		this_hostextinfo->host_name=strdup(template_hostextinfo->host_name);
	if(this_hostextinfo->hostgroup_name==NULL && template_hostextinfo->hostgroup_name!=NULL)
		this_hostextinfo->hostgroup_name=strdup(template_hostextinfo->hostgroup_name);
	if(this_hostextinfo->notes_url==NULL && template_hostextinfo->notes_url!=NULL)
		this_hostextinfo->notes_url=strdup(template_hostextinfo->notes_url);
	if(this_hostextinfo->icon_image==NULL && template_hostextinfo->icon_image!=NULL)
		this_hostextinfo->icon_image=strdup(template_hostextinfo->icon_image);
	if(this_hostextinfo->icon_image_alt==NULL && template_hostextinfo->icon_image_alt!=NULL)
		this_hostextinfo->icon_image_alt=strdup(template_hostextinfo->icon_image_alt);
	if(this_hostextinfo->vrml_image==NULL && template_hostextinfo->vrml_image!=NULL)
		this_hostextinfo->vrml_image=strdup(template_hostextinfo->vrml_image);
	if(this_hostextinfo->gd2_image==NULL && template_hostextinfo->gd2_image!=NULL)
		this_hostextinfo->gd2_image=strdup(template_hostextinfo->gd2_image);
	if(this_hostextinfo->have_2d_coords==FALSE && template_hostextinfo->have_2d_coords==TRUE){
		this_hostextinfo->x_2d=template_hostextinfo->x_2d;
		this_hostextinfo->y_2d=template_hostextinfo->y_2d;
		this_hostextinfo->have_2d_coords=TRUE;
	        }
	if(this_hostextinfo->have_3d_coords==FALSE && template_hostextinfo->have_3d_coords==TRUE){
		this_hostextinfo->x_3d=template_hostextinfo->x_3d;
		this_hostextinfo->y_3d=template_hostextinfo->y_3d;
		this_hostextinfo->z_3d=template_hostextinfo->z_3d;
		this_hostextinfo->have_3d_coords=TRUE;
	        }

#ifdef DEBUG0
	printf("xedtemplate_resolve_hostextinfo() end\n");
#endif

	return OK;
        }



/* resolves a serviceextinfo object */
int xedtemplate_resolve_serviceextinfo(xedtemplate_serviceextinfo *this_serviceextinfo){
	xedtemplate_serviceextinfo *template_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_resolve_serviceextinfo() start\n");
#endif

	/* return if this object has already been resolved */
	if(this_serviceextinfo->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_serviceextinfo->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_serviceextinfo->template==NULL)
		return OK;

	template_serviceextinfo=xedtemplate_find_serviceextinfo(this_serviceextinfo->template);
	if(template_serviceextinfo==NULL)
		return ERROR;

	/* resolve the template serviceextinfo... */
	xedtemplate_resolve_serviceextinfo(template_serviceextinfo);

	/* apply missing properties from template serviceextinfo... */
	if(this_serviceextinfo->name==NULL && template_serviceextinfo->name!=NULL)
		this_serviceextinfo->name=strdup(template_serviceextinfo->name);
	if(this_serviceextinfo->host_name==NULL && template_serviceextinfo->host_name!=NULL)
		this_serviceextinfo->host_name=strdup(template_serviceextinfo->host_name);
	if(this_serviceextinfo->hostgroup_name==NULL && template_serviceextinfo->hostgroup_name!=NULL)
		this_serviceextinfo->hostgroup_name=strdup(template_serviceextinfo->hostgroup_name);
	if(this_serviceextinfo->service_description==NULL && template_serviceextinfo->service_description!=NULL)
		this_serviceextinfo->service_description=strdup(template_serviceextinfo->service_description);
	if(this_serviceextinfo->notes_url==NULL && template_serviceextinfo->notes_url!=NULL)
		this_serviceextinfo->notes_url=strdup(template_serviceextinfo->notes_url);
	if(this_serviceextinfo->icon_image==NULL && template_serviceextinfo->icon_image!=NULL)
		this_serviceextinfo->icon_image=strdup(template_serviceextinfo->icon_image);
	if(this_serviceextinfo->icon_image_alt==NULL && template_serviceextinfo->icon_image_alt!=NULL)
		this_serviceextinfo->icon_image_alt=strdup(template_serviceextinfo->icon_image_alt);

#ifdef DEBUG0
	printf("xedtemplate_resolve_serviceextinfo() end\n");
#endif

	return OK;
        }




/******************************************************************/
/******************* OBJECT SEARCH FUNCTIONS **********************/
/******************************************************************/

/* finds a specific hostextinfo object */
xedtemplate_hostextinfo *xedtemplate_find_hostextinfo(char *name){
	xedtemplate_hostextinfo *temp_hostextinfo;

	if(name==NULL)
		return NULL;

	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if(temp_hostextinfo->name==NULL)
			continue;
		if(!strcmp(temp_hostextinfo->name,name))
			break;
	        }

	return temp_hostextinfo;
        }


/* finds a specific serviceextinfo object */
xedtemplate_serviceextinfo *xedtemplate_find_serviceextinfo(char *name){
	xedtemplate_serviceextinfo *temp_serviceextinfo;

	if(name==NULL)
		return NULL;

	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
		if(temp_serviceextinfo->name==NULL)
			continue;
		if(!strcmp(temp_serviceextinfo->name,name))
			break;
	        }

	return temp_serviceextinfo;
        }




/******************************************************************/
/**************** OBJECT REGISTRATION FUNCTIONS *******************/
/******************************************************************/

/* registers object definitions */
int xedtemplate_register_objects(void){
	xedtemplate_hostextinfo *temp_hostextinfo;
	xedtemplate_serviceextinfo *temp_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_register_objects() start\n");
#endif

	/* register hostextinfo definitions */
	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if(xedtemplate_register_hostextinfo(temp_hostextinfo)==ERROR)
			return ERROR;
	        }

	/* register serviceextinfo definitions */
	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
		if(xedtemplate_register_serviceextinfo(temp_serviceextinfo)==ERROR)
			return ERROR;
	        }

#ifdef DEBUG0
	printf("xedtemplate_register_objects() end\n");
#endif

	return OK;
        }



/* registers a hostextinfo definition */
int xedtemplate_register_hostextinfo(xedtemplate_hostextinfo *this_hostextinfo){

#ifdef DEBUG0
	printf("xedtemplate_register_hostextinfo() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostextinfo->register_object==FALSE)
		return OK;

	/* register the extended host object */
	add_extended_host_info(this_hostextinfo->host_name,this_hostextinfo->notes_url,this_hostextinfo->icon_image,this_hostextinfo->vrml_image,this_hostextinfo->gd2_image,this_hostextinfo->icon_image_alt,this_hostextinfo->x_2d,this_hostextinfo->y_2d,this_hostextinfo->x_3d,this_hostextinfo->y_3d,this_hostextinfo->z_3d,this_hostextinfo->have_2d_coords,this_hostextinfo->have_3d_coords);

#ifdef DEBUG0
	printf("xedtemplate_register_hostextinfo() end\n");
#endif

	return OK;
        }



/* registers a serviceextinfo definition */
int xedtemplate_register_serviceextinfo(xedtemplate_serviceextinfo *this_serviceextinfo){

#ifdef DEBUG0
	printf("xedtemplate_register_serviceextinfo() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_serviceextinfo->register_object==FALSE)
		return OK;

	/* register the extended service object */
	add_extended_service_info(this_serviceextinfo->host_name,this_serviceextinfo->service_description,this_serviceextinfo->notes_url,this_serviceextinfo->icon_image,this_serviceextinfo->icon_image_alt);

#ifdef DEBUG0
	printf("xedtemplate_register_serviceextinfo() end\n");
#endif

	return OK;
        }






/******************************************************************/
/********************** CLEANUP FUNCTIONS *************************/
/******************************************************************/

/* frees memory */
int xedtemplate_free_memory(void){
	xedtemplate_hostextinfo *this_hostextinfo;
	xedtemplate_hostextinfo *next_hostextinfo;
	xedtemplate_serviceextinfo *this_serviceextinfo;
	xedtemplate_serviceextinfo *next_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_free_memory() start\n");
#endif

	/* free memory allocated to hostextinfo list */
	for(this_hostextinfo=xedtemplate_hostextinfo_list;this_hostextinfo!=NULL;this_hostextinfo=next_hostextinfo){
		next_hostextinfo=this_hostextinfo->next;
		free(this_hostextinfo->template);
		free(this_hostextinfo->name);
		free(this_hostextinfo->host_name);
		free(this_hostextinfo->hostgroup_name);
		free(this_hostextinfo->notes_url);
		free(this_hostextinfo->icon_image);
		free(this_hostextinfo->icon_image_alt);
		free(this_hostextinfo->vrml_image);
		free(this_hostextinfo->gd2_image);
		free(this_hostextinfo);
	        }

	/* free memory allocated to serviceextinfo list */
	for(this_serviceextinfo=xedtemplate_serviceextinfo_list;this_serviceextinfo!=NULL;this_serviceextinfo=next_serviceextinfo){
		next_serviceextinfo=this_serviceextinfo->next;
		free(this_serviceextinfo->template);
		free(this_serviceextinfo->name);
		free(this_serviceextinfo->host_name);
		free(this_serviceextinfo->hostgroup_name);
		free(this_serviceextinfo->service_description);
		free(this_serviceextinfo->notes_url);
		free(this_serviceextinfo->icon_image);
		free(this_serviceextinfo->icon_image_alt);
		free(this_serviceextinfo);
	        }

#ifdef DEBUG0
	printf("xedtemplate_free_memory() end\n");
#endif

	return OK;
        }


/* frees memory allocated to a temporary host list */
int xedtemplate_free_hostlist(xedtemplate_hostlist *temp_list){
	xedtemplate_hostlist *this_hostlist;
	xedtemplate_hostlist *next_hostlist;

#ifdef DEBUG0
	printf("xedtemplate_free_hostlist() start\n");
#endif

	/* free memory allocated to host name list */
	for(this_hostlist=temp_list;this_hostlist!=NULL;this_hostlist=next_hostlist){
		next_hostlist=this_hostlist->next;
		free(this_hostlist->host_name);
		free(this_hostlist);
	        }

	temp_list=NULL;

#ifdef DEBUG0
	printf("xedtemplate_free_hostlist() end\n");
#endif

	return OK;
        }



/******************************************************************/
/********************** UTILITY FUNCTIONS *************************/
/******************************************************************/

/* expands a comma-delimited list of hostgroups and/or hosts to member host names */
xedtemplate_hostlist *xedtemplate_expand_hostgroups_and_hosts(char *hostgroups,char *hosts){
	xedtemplate_hostlist *temp_list;
	xedtemplate_hostlist *new_list;
	hostgroup *temp_hostgroup;
	host *temp_host;
	char *hostgroup_names;
	char *host_names;
	char *temp_ptr;
	void *host_cursor;
#ifdef USE_REGEXP_MATCHING
	regex_t preg;
	int found_match;
#endif
#ifdef NSCORE
	char temp_buffer[MAX_XEDTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xedtemplate_expand_hostgroups() start\n");
#endif

	temp_list=NULL;

	/* process list of hostgroups... */
	if(hostgroups!=NULL){

		/* allocate memory for hostgroup name list */
		hostgroup_names=strdup(hostgroups);
		if(hostgroup_names==NULL)
			return temp_list;

		for(temp_ptr=strtok(hostgroup_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){

			/* strip trailing spaces */
			xedtemplate_strip(temp_ptr);

#ifdef USE_REGEXP_MATCHING

			/* compile regular expression */
			if(regcomp(&preg,temp_ptr,0)){
				free(hostgroup_names);
				return NULL;
		                }
			
			/* test match against all hostgroup names */
			for(temp_hostgroup=hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){

				if(temp_hostgroup->group_name==NULL)
					continue;

				/* break out if this hostgroup matched the expression */
				if(!regexec(&preg,temp_hostgroup->group_name,0,NULL,0))
					break;
			        }

			/* free memory allocated to compiled regexp */
			regfree(&preg);
	
#else

			/* find the hostgroup */
			temp_hostgroup=find_hostgroup(temp_ptr,NULL);

#endif

			if(temp_hostgroup==NULL){
				continue;
				free(hostgroup_names);
				return NULL;
		                }

			/* process all hosts that belong to the hostgroup */
			host_cursor=get_host_cursor();
			while(temp_host=get_next_host_cursor(host_cursor)){

				/* skip hosts that don't belong to the hostgroup */
				if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==FALSE)
					continue;

				/* skip this host if its already in the list */
				for(new_list=temp_list;new_list;new_list=new_list->next)
					if(!strcmp(temp_host->name,new_list->host_name))
						break;
				if(new_list!=NULL)
					continue;

				/* allocate memory for a new list item */
				new_list=(xedtemplate_hostlist *)malloc(sizeof(xedtemplate_hostlist));
				if(new_list==NULL){
					free_host_cursor(host_cursor);
					free(hostgroup_names);
					return temp_list;
			                }

				/* save the host name */
				new_list->host_name=strdup(temp_host->name);
				if(new_list->host_name==NULL){
					free_host_cursor(host_cursor);
					free(hostgroup_names);
					return temp_list;
			                }

				/* add new item to head of list */
				new_list->next=temp_list;
				temp_list=new_list;
			        }

			free_host_cursor(host_cursor);
	                }

		free(hostgroup_names);
	        }

	/* process list of hosts... */
	if(hosts!=NULL){

		/* allocate memory for host name list */
		host_names=strdup(hosts);
		if(host_names==NULL)
			return temp_list;

#ifdef USE_REGEXP_MATCHING

		for(temp_ptr=strtok(host_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){
			
			/* strip trailing spaces */
			xedtemplate_strip(temp_ptr);
			
			/* compile regular expression */
			if(regcomp(&preg,temp_ptr,0)){
				free(host_names);
				return NULL;
		                }
			
			/* test match against all host names */
			found_match=FALSE;
			host_cursor=get_host_cursor();
			while(temp_host=get_next_host_cursor(host_cursor)){

				if(temp_host->name==NULL)
					continue;

				/* skip this host if it doesn't match the expression */
				if(regexec(&preg,temp_host->name,0,NULL,0))
					continue;
				else
					found_match=TRUE;

				/* skip this host if its already in the list */
				for(new_list=temp_list;new_list;new_list=new_list->next)
					if(!strcmp(temp_host->name,new_list->host_name))
						break;
				if(new_list!=NULL)
					continue;

				/* allocate memory for a new list item */
				new_list=(xedtemplate_hostlist *)malloc(sizeof(xedtemplate_hostlist));
				if(new_list==NULL){
					regfree(&preg);
					free_host_cursor(host_cursor);
					free(host_names);
					return temp_list;
		                        }

				/* save the host name */
				new_list->host_name=strdup(temp_host->name);
				if(new_list->host_name==NULL){
					regfree(&preg);
					free_host_cursor(host_cursor);
					free(host_names);
					return temp_list;
			                }
				
				/* add new item to head of list */
				new_list->next=temp_list;
				temp_list=new_list;
	                        }

			/* free memory allocated to compiled regexp */
			regfree(&preg);

			free_host_cursor(host_cursor);

			/* we didn't find a match */
			if(found_match==FALSE){
				free(host_names);
				return NULL;
			        }
		        }

#else

		/* return list of all hosts */
		if(!strcmp(host_names,"*")){

			host_cursor=get_host_cursor();
			while(temp_host=get_next_host_cursor(host_cursor)){

				if(temp_host->name==NULL)
					continue;

				/* allocate memory for a new list item */
				new_list=(xedtemplate_hostlist *)malloc(sizeof(xedtemplate_hostlist));
				if(new_list==NULL){
					free_host_cursor(host_cursor);
					free(host_names);
					return temp_list;
			                }

				/* save the host name */
				new_list->host_name=strdup(temp_host->name);
				if(new_list->host_name==NULL){
					free_host_cursor(host_cursor);
					free(host_names);
					return temp_list;
	                                }

				/* add new item to head of list */
				new_list->next=temp_list;
				temp_list=new_list;
		                }

			free_host_cursor(host_cursor);
	                }

		/* else lookup individual hosts */
		else{

			for(temp_ptr=strtok(host_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){
			
				/* strip trailing spaces */
				xedtemplate_strip(temp_ptr);
			
				/* find the host */
				temp_host=find_host(temp_ptr);
				if(temp_host==NULL){
					free(host_names);
					return NULL;
		                        }

				/* skip this host if its already in the list */
				for(new_list=temp_list;new_list;new_list=new_list->next)
					if(!strcmp(new_list->host_name,temp_ptr))
						break;
				if(new_list)
					continue;

				/* allocate memory for a new list item */
				new_list=(xedtemplate_hostlist *)malloc(sizeof(xedtemplate_hostlist));
				if(new_list==NULL){
					free(host_names);
					return temp_list;
			                }

				/* save the host name */
				new_list->host_name=strdup(temp_host->name);
				if(new_list->host_name==NULL){
					free(host_names);
					return temp_list;
	                                }

				/* add new item to head of list */
				new_list->next=temp_list;
				temp_list=new_list;
                                }
	                }
#endif

		free(host_names);
	        }

#ifdef DEBUG0
	printf("xedtemplate_expand_hostgroups_and_hosts() end\n");
#endif

	return temp_list;
        }
