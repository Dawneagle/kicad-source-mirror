/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright 2016-2017 CERN
 * Copyright (C) 2021 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <core/kicad_algo.h>
#include <commit.h>
#include <eda_item.h>
#include <macros.h>

COMMIT::COMMIT()
{
}


COMMIT::~COMMIT()
{
    for( COMMIT_LINE& ent : m_changes )
    {
        if( ent.m_copy )
            delete ent.m_copy;
    }
}


COMMIT& COMMIT::Stage( EDA_ITEM* aItem, CHANGE_TYPE aChangeType, BASE_SCREEN* aScreen )
{
    // CHT_MODIFY and CHT_DONE are not compatible
    wxASSERT( ( aChangeType & ( CHT_MODIFY | CHT_DONE ) ) != ( CHT_MODIFY | CHT_DONE ) );

    int flag = aChangeType & CHT_FLAGS;

    switch( aChangeType & CHT_TYPE )
    {
    case CHT_ADD:
        wxASSERT( m_changedItems.find( aItem ) == m_changedItems.end() );
        makeEntry( aItem, CHT_ADD | flag, nullptr, aScreen );
        return *this;

    case CHT_REMOVE:
        makeEntry( aItem, CHT_REMOVE | flag, nullptr, aScreen );
        return *this;

    case CHT_MODIFY:
    {
        EDA_ITEM* parent = parentObject( aItem );
        EDA_ITEM* clone = makeImage( parent );

        wxASSERT( clone );

        if( clone )
            return createModified( parent, clone, flag, aScreen );

        break;
    }

    default:
        wxFAIL;
    }

    return *this;
}


COMMIT& COMMIT::Stage( std::vector<EDA_ITEM*> &container, CHANGE_TYPE aChangeType,
        BASE_SCREEN *aScreen )
{
    for( EDA_ITEM* item : container )
        Stage( item, aChangeType, aScreen);

    return *this;
}


COMMIT& COMMIT::Stage( const PICKED_ITEMS_LIST &aItems, UNDO_REDO aModFlag, BASE_SCREEN *aScreen )
{
    for( unsigned int i = 0; i < aItems.GetCount(); i++ )
    {
        UNDO_REDO change_type = aItems.GetPickedItemStatus( i );
        EDA_ITEM* item = aItems.GetPickedItem( i );

        if( change_type == UNDO_REDO::UNSPECIFIED )
            change_type = aModFlag;

        if( EDA_ITEM* copy = aItems.GetPickedItemLink( i ) )
        {
            assert( change_type == UNDO_REDO::CHANGED );

            // There was already a copy created, so use it
            Modified( item, copy, aScreen );
        }
        else
        {
            Stage( item, convert( change_type ), aScreen );
        }
    }

    return *this;
}


int COMMIT::GetStatus( EDA_ITEM* aItem, BASE_SCREEN *aScreen )
{
    COMMIT_LINE* entry = findEntry( parentObject( aItem ), aScreen );

    return entry ? entry->m_type : 0;
}


COMMIT& COMMIT::createModified( EDA_ITEM* aItem, EDA_ITEM* aCopy, int aExtraFlags, BASE_SCREEN* aScreen )
{
    EDA_ITEM* parent = parentObject( aItem );
    auto entryIt = m_changedItems.find( parent );

    if( entryIt != m_changedItems.end() )
    {
        delete aCopy;
        return *this; // item has been already modified once
    }

    makeEntry( parent, CHT_MODIFY | aExtraFlags, aCopy, aScreen );

    return *this;
}


void COMMIT::makeEntry( EDA_ITEM* aItem, CHANGE_TYPE aType, EDA_ITEM* aCopy, BASE_SCREEN *aScreen )
{
    // Expect an item copy if it is going to be modified
    wxASSERT( !!aCopy == ( ( aType & CHT_TYPE ) == CHT_MODIFY ) );

    if( m_changedItems.find( aItem ) != m_changedItems.end() )
    {
        alg::delete_if( m_changes, [aItem, aScreen]( const COMMIT_LINE& aEnt )
                                   {
                                       return aEnt.m_item == aItem && aEnt.m_screen == aScreen;
                                   } );
    }

    COMMIT_LINE ent;

    ent.m_item = aItem;
    ent.m_type = aType;
    ent.m_copy = aCopy;
    ent.m_screen = aScreen;

    m_changedItems.insert( aItem );
    m_changes.push_back( ent );
}


COMMIT::COMMIT_LINE* COMMIT::findEntry( EDA_ITEM* aItem, BASE_SCREEN *aScreen )
{
    for( COMMIT_LINE& change : m_changes )
    {
        if( change.m_item == aItem && change.m_screen == aScreen )
            return &change;
    }

    return nullptr;
}


CHANGE_TYPE COMMIT::convert( UNDO_REDO aType ) const
{
    switch( aType )
    {
    case UNDO_REDO::NEWITEM:
        return CHT_ADD;

    case UNDO_REDO::DELETED:
        return CHT_REMOVE;

    default:
        assert( false );
        // Can't fall through if the assert fires, so quiet our warning
#ifdef NDEBUG
        KI_FALLTHROUGH;
#endif

    case UNDO_REDO::CHANGED:
        return CHT_MODIFY;
    }
}

