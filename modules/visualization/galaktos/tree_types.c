/*****************************************************************************
 * tree_types.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          code from projectM http://xmms-projectm.sourceforge.net
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

/* Compares integer value numbers in 32 bit range */
int compare_int(int * num1, int * num2) {

	if ((*num1) < (*num2))
		return -1;
	if ((*num1) > (*num2))
		return 1;
	
	return 0;
}

/* Compares strings in lexographical order */
int compare_string(char * str1, char * str2) {

  //  printf("comparing \"%s\" to \"%s\"\n", str1, str2);
  //return strcmp(str1, str2);
  return strncmp(str1, str2, MAX_TOKEN_SIZE-1);
	
}	

/* Compares a string in version order. That is, file1 < file2 < file10 */
int compare_string_version(char * str1, char * str2) {

  return strverscmp(str1, str2);

}


void free_int(void * num) {
	free(num);
}


void free_string(char * string) {
	
	free(string);	
}	
 
void * copy_int(int * num) {
	
	int * new_num;
	
	if ((new_num = (int*)malloc(sizeof(int))) == NULL)
		return NULL;

	*new_num = *num;
	
	return (void*)new_num;
}	


void * copy_string(char * string) {
	
	char * new_string;
	
	if ((new_string = (char*)malloc(MAX_TOKEN_SIZE)) == NULL)
		return NULL;
	
	strncpy(new_string, string, MAX_TOKEN_SIZE-1);
	
	return (void*)new_string;
}
