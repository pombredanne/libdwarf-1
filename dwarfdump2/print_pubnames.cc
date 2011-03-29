/* 
  Copyright (C) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright 2007-2010 Sun Microsystems, Inc. All rights reserved.
  Portions Copyright 2009-2010 SN Systems Ltd. All rights reserved.
  Portions Copyright 2008-2011 David Anderson. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement
  or the like.  Any license provided herein, whether implied or
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with
  other software, or any other product whatsoever.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 51
  Franklin Street - Fifth Floor, Boston MA 02110-1301, USA.

  Contact information:  Silicon Graphics, Inc., 1500 Crittenden Lane,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan



$Header: /plroot/cmplrs.src/v7.4.5m/.RCS/PL/dwarfdump/RCS/print_sections.c,v 1.69 2006/04/17 00:09:56 davea Exp $ */
/*  The address of the Free Software Foundation is
    Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, 
    Boston, MA 02110-1301, USA.  
    SGI has moved from the Crittenden Lane address.
*/

#include "globals.h"
#include <vector>
#include "naming.h"
#include "dwconf.h"

#include "print_sections.h"
#include "print_frames.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

/*  This unifies the code for some error checks to
    avoid code duplication.
*/
void
check_info_offset_sanity(const string & sec,
    const string &field,
    const string &global,
    Dwarf_Unsigned offset, Dwarf_Unsigned maxoff)
{
    if (maxoff == 0) {
        /* Lets make a heuristic check. */
        if (offset > 0xffffffff) {
            cout << "Warning: section " << sec << " " <<
                field << " " << global;
            cout << " offset" << IToHex(offset);
            cout << " exceptionally large" << endl;
        }
        return;
    }
    if (offset >= maxoff) {
        cout << "Warning: section " << sec << " " <<
            field << " " << global;
        cout << " offset "<< IToHex(offset);
        cout << " larger than max of " << IToHex(maxoff) << endl;
    }
}



/*  Unified pubnames style output.
    The error checking here against maxoff may be useless
    (in that libdwarf may return an error if the offset is bad
    and we will not get called here).
    But we leave it in nonetheless as it looks sensible.
    In at least one gigantic executable such offsets turned out wrong.
*/
void
print_pubname_style_entry(Dwarf_Debug dbg,
    const string & line_title,
    const string & name,
    Dwarf_Unsigned die_off,
    Dwarf_Unsigned cu_off,
    Dwarf_Unsigned global_cu_offset,
    Dwarf_Unsigned maxoff)
{
    Dwarf_Die die = NULL;
    Dwarf_Die cu_die = NULL;
    Dwarf_Off die_CU_off = 0;

    /* get die at die_off */
    int dres = dwarf_offdie(dbg, die_off, &die, &err);
    if (dres != DW_DLV_OK) {
        string details = string(line_title) + string(" dwarf_offdie : "
            "die offset does not reference valid DIE.  ")
 
            + IToHex(die_off,0) +
            string(".");
        print_error(dbg, details.c_str(), dres, err);
    }


    /* get offset of die from its cu-header */
    int ddres = dwarf_die_CU_offset(die, &die_CU_off, &err);
    if (ddres != DW_DLV_OK) {
        string details = string(line_title) + " cannot get CU die offset";
        print_error(dbg, details.c_str(), ddres, err);
    }

    /* get die at offset cu_off */
    int cudres = dwarf_offdie(dbg, cu_off, &cu_die, &err);
    if (cudres != DW_DLV_OK) {
        string details =  string(line_title) + string(" dwarf_offdie: "
            "die offset does not reference valid CU DIE.  ")
            + IToHex(cu_off,0) +
            string(".");
        dwarf_dealloc(dbg, die, DW_DLA_DIE);
        print_error(dbg, details.c_str(), cudres, err);
    }
    cout << line_title ;
    cout << " " <<  LeftAlign(15,name);
    cout << " die-in-sect " <<  die_off;
    cout << ", cu-in-sect " <<  cu_off;
    cout << ", die-in-cu " <<  die_CU_off;
    cout << ", cu-header-in-sect " << ((Dwarf_Signed) (die_off - die_CU_off));

    if ((die_off - die_CU_off) != global_cu_offset) {
        cout << line_title <<  " error: real cuhdr "<<  global_cu_offset << endl;
        exit(1);
    }
    if (verbose) {
        cout  << " cuhdr " << global_cu_offset;
    }
    cout << endl;

    dwarf_dealloc(dbg, die, DW_DLA_DIE);
    dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);

    check_info_offset_sanity(line_title,
        "die offset", name, die_off, maxoff);
    check_info_offset_sanity(line_title,
        "die cu offset", name, die_CU_off, maxoff);
    check_info_offset_sanity(line_title,
        "cu offset", name,
        (die_off - die_CU_off), maxoff);
}


/* get all the data in .debug_pubnames */
extern void
print_pubnames(Dwarf_Debug dbg)
{
    Dwarf_Global *globbuf = NULL;
    Dwarf_Signed count = 0;
    Dwarf_Signed i = 0;
    Dwarf_Off die_off = 0;
    Dwarf_Off cu_off = 0;
    char *name = 0;
    int res = 0;

    cout << endl; 
    cout << ".debug_pubnames" << endl;
    res = dwarf_get_globals(dbg, &globbuf, &count, &err);
    if (res == DW_DLV_ERROR) {
        print_error(dbg, "dwarf_get_globals", res, err);
    } else if (res == DW_DLV_NO_ENTRY) {
        /*  (err == 0 && count == DW_DLV_NOCOUNT) means there are no
            pubnames.  */
    } else {
        Dwarf_Unsigned maxoff = get_info_max_offset(dbg);

        for (i = 0; i < count; i++) {
            int nres;
            int cures3;
            Dwarf_Off global_cu_off = 0;

            nres = dwarf_global_name_offsets(globbuf[i],
                &name, &die_off, &cu_off, &err);
            deal_with_name_offset_err(dbg, "pubnames dwarf_global_name_offsets",
                name, die_off, nres, err);
            cures3 = dwarf_global_cu_offset(globbuf[i],
                &global_cu_off, &err);
            if (cures3 != DW_DLV_OK) {
                print_error(dbg, "pubnames dwarf_global_cu_offset", 
                    cures3, err);
            }

            print_pubname_style_entry(dbg,
                "global",
                name, die_off, cu_off,
                global_cu_off, maxoff);

            /* print associated die too? */

            if (check_pubname_attr) {
                Dwarf_Bool has_attr;
                int ares;
                int dres;
                Dwarf_Die die;

                /* get die at die_off */
                dres = dwarf_offdie(dbg, die_off, &die, &err);
                if (dres != DW_DLV_OK) {
                    string details = string("dwarf_offdie in "
                        "checking pubnames attribute: "
                        "die offset does not reference valid DIE.  ")
                        + IToHex(die_off,0) +
                        string(".");
                    print_error(dbg, details.c_str(), dres, err);
                    print_error(dbg, "dwarf_offdie", dres, err);
                }


                ares =
                    dwarf_hasattr(die, DW_AT_external, &has_attr, &err);
                if (ares == DW_DLV_ERROR) {
                    print_error(dbg, "pubnames hassattr on DW_AT_external", ares,
                        err);
                }
                pubname_attr_result.checks++;
                if (ares == DW_DLV_OK && has_attr) {
                    /* Should the value of flag be examined? */
                } else {
                    DWARF_CHECK_ERROR2(pubname_attr_result,name,
                        "pubname does not have DW_AT_external")
                }
                dwarf_dealloc(dbg, die, DW_DLA_DIE);
            }
        }
        dwarf_globals_dealloc(dbg, globbuf, count);
    }
}                               /* print_pubnames() */

