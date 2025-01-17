/*
 * KiRouter - a push-and-(sometimes-)shove PCB router
 *
 * Copyright (C) 2013-2014 CERN
 * Copyright (C) 2016-2023 KiCad Developers, see AUTHORS.txt for contributors.
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pns_logger.h"
#include "pns_item.h"
#include "pns_via.h"

#include <wx/log.h>

namespace PNS {

LOGGER::LOGGER( )
{
}


LOGGER::~LOGGER()
{
}


void LOGGER::Clear()
{
    m_events.clear();
}


void LOGGER::Save( const std::string& aFilename )
{
    FILE* f = fopen( aFilename.c_str(), "wb" );

    wxLogTrace( wxT( "PNS" ), wxT( "Saving to '%s' [%p]" ), aFilename.c_str(), f );

    for( const EVENT_ENTRY& evt : m_events )
    {
        uint64_t id = 0;

        fprintf( f, "event %d %d %d %s\n", evt.type, evt.p.x, evt.p.y,
                 static_cast<const char*>( evt.uuid.AsString().c_str() ) );
    }

    fclose( f );
}


void LOGGER::Log( LOGGER::EVENT_TYPE evt, const VECTOR2I& pos, const ITEM* item,
                  const SIZES_SETTINGS* sizes )
{
    LOGGER::EVENT_ENTRY ent;

    ent.type = evt;
    ent.p = pos;
    ent.uuid = KIID( 0 );

    if( sizes )
    {
        ent.sizes = *sizes;
    }

    if( item && item->Parent() )
        ent.uuid = item->Parent()->m_Uuid;

    m_events.push_back( ent );
}

}
