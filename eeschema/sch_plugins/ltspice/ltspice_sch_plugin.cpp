/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2022 Chetan Subhash Shinde<chetanshinde2001@gmail.com>
 * Copyright (C) 2023 CERN
 * Copyright (C) 2022-2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sch_plugins/ltspice/ltspice_schematic.h>
#include <sch_plugins/ltspice/ltspice_sch_plugin.h>
#include <sch_plugins/ltspice/ltspice_sch_parser.h>

#include <schematic.h>
#include <sch_sheet.h>
#include <sch_screen.h>
#include <sch_line.h>
#include <wildcards_and_files_ext.h>

/**
 * @brief schematic PLUGIN for Ltspice (*.asc) and (.asy) format.
 */

const wxString SCH_LTSPICE_PLUGIN::GetName() const
{
    return wxT( "Ltspice Schematic Importer" );
}


const wxString SCH_LTSPICE_PLUGIN::GetFileExtension() const
{
    return wxT( "asc" );
}


const wxString SCH_LTSPICE_PLUGIN::GetLibraryFileExtension() const
{
    return wxT( "lib" );
}


int SCH_LTSPICE_PLUGIN::GetModifyHash() const
{
    return 0;
}


SCH_SHEET* SCH_LTSPICE_PLUGIN::Load( const wxString& aFileName, SCHEMATIC* aSchematic,
                                     SCH_SHEET* aAppendToMe, const STRING_UTF8_MAP* aProperties )
{
    wxASSERT( !aFileName || aSchematic );

    SCH_SHEET* rootSheet = nullptr;

    if( aAppendToMe )
    {
        wxCHECK_MSG( aSchematic->IsValid(), nullptr, "Can't append to a schematic with no root!" );
        rootSheet = &aSchematic->Root();
    }
    else
    {
        rootSheet = new SCH_SHEET( aSchematic );
        rootSheet->SetFileName( aFileName );
        aSchematic->SetRoot( rootSheet );
    }

    if( !rootSheet->GetScreen() )
    {
        SCH_SCREEN* screen = new SCH_SCREEN( aSchematic );

        screen->SetFileName( aFileName );
        rootSheet->SetScreen( screen );
    }

    SYMBOL_LIB_TABLE* libTable = aSchematic->Prj().SchSymbolLibTable();

    wxCHECK_MSG( libTable, nullptr, "Could not load symbol lib table." );

    LTSPICE_SCHEMATIC               ascFile( aFileName );
    SCH_PLUGIN::SCH_PLUGIN_RELEASER sch_plugin;

    sch_plugin.set( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );

    ascFile.Load( aSchematic, rootSheet, &sch_plugin, aFileName );

    aSchematic->CurrentSheet().UpdateAllScreenReferences();

    return rootSheet;
}


bool SCH_LTSPICE_PLUGIN::CheckHeader( const wxString& aFileName )
{
    return true;
}
