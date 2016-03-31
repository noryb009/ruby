/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/
#ifndef rbomrddr_h
#define rbomrddr_h

enum POI_TYPE {
	RUBY_VM_POI = 0,
	GLOBAL_SYMTAB_POI,
	RUBY_COBJECT_POI,
};

typedef struct RubyPOIListEntry {
	int poiType;
	char description[32];
	void *poiPtr;
	struct RubyPOIListEntry* next;
} RubyPOIListEntry;

typedef struct J9RAS {
	char id[8];					/* this is the RAS eyecatcher J9VMRAS */
	uint32_t check1;			/* additional check digits - prevents mistaken detection of eyecatcher when walking memory */
	uint32_t check2;
	int32_t version;			/* version of this structure */
	int32_t length;				/* length of this structure */
	void* blob;					/* where the blob has been loaded in memory, or -1 if this structure directly precedes the blob in memory */
	struct RubyPOIListEntry* poiList;	/* pointer to the Points of Interest */
} J9RAS;

#endif		/* rbomrddr_h */
