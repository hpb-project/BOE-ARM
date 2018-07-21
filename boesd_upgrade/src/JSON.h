/*
  Copyright (c) 2009 Dave Gamble
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef JSON__h
#define JSON__h

#ifdef __cplusplus
extern "C"
{
#endif

/* JSON Types: */
#define JSON_False 0
#define JSON_True 1
#define JSON_NULL 2
#define JSON_Number 3
#define JSON_String 4
#define JSON_Array 5
#define JSON_Object 6
	
#define JSON_IsReference 256
#define JSON_StringIsConst 512

/* The JSON structure: */
typedef struct JSON {
	struct JSON *next,*prev;	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct JSON *child;		/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;					/* The type of the item, as above. */

	char *valuestring;			/* The item's string, if type==JSON_String */
	int valueint;				/* The item's number, if type==JSON_Number */
	double valuedouble;			/* The item's number, if type==JSON_Number */

	char *string;				/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
} JSON;

typedef struct JSON_Hooks {
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} JSON_Hooks;

/* Supply malloc, realloc and free functions to JSON */
extern void JSON_InitHooks(JSON_Hooks* hooks);


/* Supply a block of JSON, and this returns a JSON object you can interrogate. Call JSON_Delete when finished. */
extern JSON *JSON_Parse(const char *value);
/* Render a JSON entity to text for transfer/storage. Free the char* when finished. */
extern char  *JSON_Print(JSON *item);
/* Render a JSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
extern char  *JSON_PrintUnformatted(JSON *item);
/* Render a JSON entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
extern char *JSON_PrintBuffered(JSON *item,int prebuffer,int fmt);
/* Delete a JSON entity and all subentities. */
extern void   JSON_Delete(JSON *c);

/* Returns the number of items in an array (or object). */
extern int	  JSON_GetArraySize(JSON *array);
/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern JSON *JSON_GetArrayItem(JSON *array,int item);
/* Get item "string" from object. Case insensitive. */
extern JSON *JSON_GetObjectItem(JSON *object,const char *string);

/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when JSON_Parse() returns 0. 0 when JSON_Parse() succeeds. */
extern const char *JSON_GetErrorPtr(void);
	
/* These calls create a JSON item of the appropriate type. */
extern JSON *JSON_CreateNull(void);
extern JSON *JSON_CreateTrue(void);
extern JSON *JSON_CreateFalse(void);
extern JSON *JSON_CreateBool(int b);
extern JSON *JSON_CreateNumber(double num);
extern JSON *JSON_CreateString(const char *string);
extern JSON *JSON_CreateArray(void);
extern JSON *JSON_CreateObject(void);

/* These utilities create an Array of count items. */
extern JSON *JSON_CreateIntArray(const int *numbers,int count);
extern JSON *JSON_CreateFloatArray(const float *numbers,int count);
extern JSON *JSON_CreateDoubleArray(const double *numbers,int count);
extern JSON *JSON_CreateStringArray(const char **strings,int count);

/* Append item to the specified array/object. */
extern void JSON_AddItemToArray(JSON *array, JSON *item);
extern void	JSON_AddItemToObject(JSON *object,const char *string,JSON *item);
extern void	JSON_AddItemToObjectCS(JSON *object,const char *string,JSON *item);	/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the JSON object */
/* Append reference to item to the specified array/object. Use this when you want to add an existing JSON to a new JSON, but don't want to corrupt your existing JSON. */
extern void JSON_AddItemReferenceToArray(JSON *array, JSON *item);
extern void	JSON_AddItemReferenceToObject(JSON *object,const char *string,JSON *item);

/* Remove/Detatch items from Arrays/Objects. */
extern JSON *JSON_DetachItemFromArray(JSON *array,int which);
extern void   JSON_DeleteItemFromArray(JSON *array,int which);
extern JSON *JSON_DetachItemFromObject(JSON *object,const char *string);
extern void   JSON_DeleteItemFromObject(JSON *object,const char *string);
	
/* Update array items. */
extern void JSON_InsertItemInArray(JSON *array,int which,JSON *newitem);	/* Shifts pre-existing items to the right. */
extern void JSON_ReplaceItemInArray(JSON *array,int which,JSON *newitem);
extern void JSON_ReplaceItemInObject(JSON *object,const char *string,JSON *newitem);

/* Duplicate a JSON item */
extern JSON *JSON_Duplicate(JSON *item,int recurse);
/* Duplicate will create a new, identical JSON item to the one you pass, in new memory that will
need to be released. With recurse!=0, it will duplicate any children connected to the item.
The item->next and ->prev pointers are always zero on return from Duplicate. */

/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
extern JSON *JSON_ParseWithOpts(const char *value,const char **return_parse_end,int require_null_terminated);

extern void JSON_Minify(char *json);

/* Macros for creating things quickly. */
#define JSON_AddNullToObject(object,name)		JSON_AddItemToObject(object, name, JSON_CreateNull())
#define JSON_AddTrueToObject(object,name)		JSON_AddItemToObject(object, name, JSON_CreateTrue())
#define JSON_AddFalseToObject(object,name)		JSON_AddItemToObject(object, name, JSON_CreateFalse())
#define JSON_AddBoolToObject(object,name,b)	JSON_AddItemToObject(object, name, JSON_CreateBool(b))
#define JSON_AddNumberToObject(object,name,n)	JSON_AddItemToObject(object, name, JSON_CreateNumber(n))
#define JSON_AddStringToObject(object,name,s)	JSON_AddItemToObject(object, name, JSON_CreateString(s))

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define JSON_SetIntValue(object,val)			((object)?(object)->valueint=(object)->valuedouble=(val):(val))
#define JSON_SetNumberValue(object,val)		((object)?(object)->valueint=(object)->valuedouble=(val):(val))

#ifdef __cplusplus
}
#endif

#endif
