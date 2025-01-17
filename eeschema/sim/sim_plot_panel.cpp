/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2016-2023 CERN
 * Copyright (C) 2021-2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * https://www.gnu.org/licenses/gpl-3.0.html
 * or you may search the http://www.gnu.org website for the version 3 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "sim_plot_colors.h"
#include "sim_plot_panel.h"
#include "simulator_frame.h"
#include "core/kicad_algo.h"

#include <algorithm>
#include <limits>


static wxString formatFloat( double x, int nDigits )
{
    wxString rv, fmt;

    if( nDigits )
        fmt.Printf( "%%.0%df", nDigits );
    else
        fmt = wxT( "%.0f" );

    rv.Printf( fmt, x );

    return rv;
}


static void getSISuffix( double x, const wxString& unit, int& power, wxString& suffix )
{
    const int n_powers = 11;

    const struct
    {
        int  exponent;
        char suffix;
    } powers[] =
    {
        { -18, 'a' },
        { -15, 'f' },
        { -12, 'p' },
        { -9,  'n' },
        { -6,  'u' },
        { -3,  'm' },
        { 0,   0   },
        { 3,   'k' },
        { 6,   'M' },
        { 9,   'G' },
        { 12,  'T' },
        { 14,  'P' }
    };

    power = 0;
    suffix = unit;

    if( x == 0.0 )
        return;

    for( int i = 0; i < n_powers - 1; i++ )
    {
        double r_cur = pow( 10, powers[i].exponent );

        if( fabs( x ) >= r_cur && fabs( x ) < r_cur * 1000.0 )
        {
            power = powers[i].exponent;

            if( powers[i].suffix )
                suffix = wxString( powers[i].suffix ) + unit;
            else
                suffix = unit;

            return;
        }
    }
}


static int countDecimalDigits( double x, int maxDigits )
{
    if( std::isnan( x ) )
    {
        // avoid trying to count the decimals of NaN
        return 0;
    }

    int64_t k = (int)( ( x - floor( x ) ) * pow( 10.0, (double) maxDigits ) );
    int n = 0;

    while( k && ( ( k % 10LL ) == 0LL || ( k % 10LL ) == 9LL ) )
    {
        k /= 10LL;
    }

    n = 0;

    while( k != 0LL )
    {
        n++;
        k /= 10LL;
    }

    return n;
}


template <typename parent>
class LIN_SCALE : public parent
{
public:
    LIN_SCALE( const wxString& name, const wxString& unit, int flags ) :
            parent( name, flags, false ),
            m_unit( unit )
    {};

    wxString GetUnits() const { return m_unit; }

private:
    void formatLabels() override
    {
        double        maxVis = parent::AbsVisibleMaxValue();

        wxString      suffix;
        int           power = 0;
        int           digits = 0;
        int constexpr DIGITS = 3;

        getSISuffix( maxVis, m_unit, power, suffix );

        double sf = pow( 10.0, power );

        for( mpScaleBase::TickLabel& l : parent::TickLabels() )
        {
            int k = countDecimalDigits( l.pos / sf, DIGITS );

            digits = std::max( digits, k );
        }

        for( mpScaleBase::TickLabel& l : parent::TickLabels() )
        {
            l.label = formatFloat( l.pos / sf, digits ) + suffix;
            l.visible = true;
        }
    }

private:
    const wxString m_unit;
};


template <typename parent>
class LOG_SCALE : public parent
{
public:
    LOG_SCALE( const wxString& name, const wxString& unit, int flags ) :
            parent( name, flags, false ),
            m_unit( unit )
    {};

    wxString GetUnits() const { return m_unit; }

private:
    void formatLabels() override
    {
        wxString suffix;
        int      power;

        for( mpScaleBase::TickLabel& l : parent::TickLabels() )
        {
            getSISuffix( l.pos, m_unit, power, suffix );
            double sf = pow( 10.0, power );
            int    k = countDecimalDigits( l.pos / sf, 3 );

            l.label = formatFloat( l.pos / sf, k ) + suffix;
            l.visible = true;
        }
    }

private:
    const wxString m_unit;
};


void CURSOR::SetCoordX( double aValue )
{
    wxRealPoint oldCoords = m_coords;

    doSetCoordX( aValue );
    m_updateRequired = false;
    m_updateRef = true;

    if( m_window )
    {
        wxRealPoint delta = m_coords - oldCoords;
        mpInfoLayer::Move( wxPoint( m_window->x2p( m_trace->x2s( delta.x ) ),
                                    m_window->y2p( m_trace->y2s( delta.y ) ) ) );

        m_window->Refresh();
    }
}


void CURSOR::doSetCoordX( double aValue )
{
    m_coords.x = aValue;

    const std::vector<double>& dataX = m_trace->GetDataX();
    const std::vector<double>& dataY = m_trace->GetDataY();

    if( dataX.size() <= 1 )
        return;

    // Find the closest point coordinates
    auto maxXIt = std::upper_bound( dataX.begin(), dataX.end(), m_coords.x );
    int maxIdx = maxXIt - dataX.begin();
    int minIdx = maxIdx - 1;

    // Out of bounds checks
    if( minIdx < 0 )
    {
        minIdx = 0;
        maxIdx = 1;
        m_coords.x = dataX[0];
    }
    else if( maxIdx >= (int) dataX.size() )
    {
        maxIdx = dataX.size() - 1;
        minIdx = maxIdx - 1;
        m_coords.x = dataX[maxIdx];
    }

    const double leftX = dataX[minIdx];
    const double rightX = dataX[maxIdx];
    const double leftY = dataY[minIdx];
    const double rightY = dataY[maxIdx];

    // Linear interpolation
    m_coords.y = leftY + ( rightY - leftY ) / ( rightX - leftX ) * ( m_coords.x - leftX );
}


wxString CURSOR::getID()
{
    for( const auto& [ id, cursor ] : m_trace->GetCursors() )
    {
        if( cursor == this )
            return wxString::Format( _( "%d" ), id );
    }

    return wxEmptyString;
}


void CURSOR::Plot( wxDC& aDC, mpWindow& aWindow )
{
    if( !m_window )
        m_window = &aWindow;

    if( !m_visible || m_trace->GetDataX().size() <= 1 )
        return;

    if( m_updateRequired )
    {
        doSetCoordX( m_trace->s2x( aWindow.p2x( m_dim.x ) ) );
        m_updateRequired = false;

        // Notify the parent window about the changes
        wxQueueEvent( aWindow.GetParent(), new wxCommandEvent( EVT_SIM_CURSOR_UPDATE ) );
    }
    else
    {
        m_updateRef = true;
    }

    if( m_updateRef )
    {
        UpdateReference();
        m_updateRef = false;
    }

    // Line length in horizontal and vertical dimensions
    const wxPoint cursorPos( aWindow.x2p( m_trace->x2s( m_coords.x ) ),
                             aWindow.y2p( m_trace->y2s( m_coords.y ) ) );

    wxCoord leftPx   = m_drawOutsideMargins ? 0 : aWindow.GetMarginLeft();
    wxCoord rightPx  = m_drawOutsideMargins ? aWindow.GetScrX() :
                                              aWindow.GetScrX() - aWindow.GetMarginRight();
    wxCoord topPx    = m_drawOutsideMargins ? 0 : aWindow.GetMarginTop();
    wxCoord bottomPx = m_drawOutsideMargins ? aWindow.GetScrY() :
                                              aWindow.GetScrY() - aWindow.GetMarginBottom();

    wxPen    pen = GetPen();
    wxColour fg = GetPen().GetColour();

    pen.SetColour( COLOR4D( m_trace->GetTraceColour() ).Mix( fg, 0.6 ).ToColour() );
    pen.SetStyle( m_continuous ? wxPENSTYLE_SOLID : wxPENSTYLE_LONG_DASH );
    aDC.SetPen( pen );

    if( topPx < cursorPos.y && cursorPos.y < bottomPx )
        aDC.DrawLine( leftPx, cursorPos.y, rightPx, cursorPos.y );

    if( leftPx < cursorPos.x && cursorPos.x < rightPx )
    {
        aDC.DrawLine( cursorPos.x, topPx, cursorPos.x, bottomPx );

        wxString id = getID();
        wxSize   size = aDC.GetTextExtent( wxS( "M" ) );
        wxRect   textRect( wxPoint( cursorPos.x + 1 - size.x / 2, topPx - 4 - size.y ), size );
        wxBrush  brush;
        wxPoint  poly[3];

        // Because a "1" looks off-center if it's actually centred.
        if( id == "1" )
            textRect.x -= 1;

        // We want an equalateral triangle, so use size.y for both axes.
        size.y += 3;
        // Make sure it's an even number so the slopes of the sides will be identical.
        size.y = ( size.y / 2 ) * 2;
        poly[0] = { cursorPos.x - 1 - size.y / 2, topPx - size.y };
        poly[1] = { cursorPos.x + 1 + size.y / 2, topPx - size.y };
        poly[2] = { cursorPos.x, topPx };

        brush.SetStyle( wxBRUSHSTYLE_SOLID );
        brush.SetColour( m_trace->GetTraceColour() );
        aDC.SetBrush( brush );
        aDC.DrawPolygon( 3, poly );

        aDC.SetTextForeground( fg );
        aDC.DrawLabel( id, textRect, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL );
    }
}


bool CURSOR::Inside( const wxPoint& aPoint ) const
{
    if( !m_window || !m_trace )
        return false;

    return ( std::abs( (double) aPoint.x -
                       m_window->x2p( m_trace->x2s( m_coords.x ) ) ) <= DRAG_MARGIN )
        || ( std::abs( (double) aPoint.y -
                       m_window->y2p( m_trace->y2s( m_coords.y ) ) ) <= DRAG_MARGIN );
}


void CURSOR::UpdateReference()
{
    if( !m_window )
        return;

    m_reference.x = m_window->x2p( m_trace->x2s( m_coords.x ) );
    m_reference.y = m_window->y2p( m_trace->y2s( m_coords.y ) );
}


SIM_PLOT_PANEL::SIM_PLOT_PANEL( const wxString& aCommand, int aOptions, wxWindow* parent,
                                wxWindowID id, const wxPoint& pos, const wxSize& size, long style,
                                const wxString& name ) :
        SIM_PANEL_BASE( aCommand, aOptions, parent, id, pos, size, style, name ),
        m_axis_x( nullptr ),
        m_axis_y1( nullptr ),
        m_axis_y2( nullptr ),
        m_axis_y3( nullptr ),
        m_dotted_cp( false )
{
    m_sizer   = new wxBoxSizer( wxVERTICAL );
    m_plotWin = new mpWindow( this, wxID_ANY, pos, size, style );

    m_plotWin->LimitView( true );
    m_plotWin->SetMargins( 35, 70, 35, 70 );

    UpdatePlotColors();

    updateAxes();

    // a mpInfoLegend displays le name of traces on the left top panel corner:
    m_legend = new mpInfoLegend( wxRect( 0, 40, 200, 40 ), wxTRANSPARENT_BRUSH );
    m_legend->SetVisible( false );
    m_plotWin->AddLayer( m_legend );

    m_plotWin->EnableDoubleBuffer( true );
    m_plotWin->UpdateAll();

    m_sizer->Add( m_plotWin, 1, wxALL | wxEXPAND, 1 );
    SetSizer( m_sizer );
}


SIM_PLOT_PANEL::~SIM_PLOT_PANEL()
{
    // ~mpWindow destroys all the added layers, so there is no need to destroy m_traces contents
}


wxString SIM_PLOT_PANEL::GetUnitsX() const
{
    LOG_SCALE<mpScaleXLog>* logScale = dynamic_cast<LOG_SCALE<mpScaleXLog>*>( m_axis_x );
    LIN_SCALE<mpScaleX>*    linScale = dynamic_cast<LIN_SCALE<mpScaleX>*>( m_axis_x );

    if( logScale )
        return logScale->GetUnits();
    else if( linScale )
        return linScale->GetUnits();
    else
        return wxEmptyString;
}


wxString SIM_PLOT_PANEL::GetUnitsY1() const
{
    LIN_SCALE<mpScaleY>* linScale = dynamic_cast<LIN_SCALE<mpScaleY>*>( m_axis_y1 );

    if( linScale )
        return linScale->GetUnits();
    else
        return wxEmptyString;
}


wxString SIM_PLOT_PANEL::GetUnitsY2() const
{
    LIN_SCALE<mpScaleY>* linScale = dynamic_cast<LIN_SCALE<mpScaleY>*>( m_axis_y2 );

    if( linScale )
        return linScale->GetUnits();
    else
        return wxEmptyString;
}


wxString SIM_PLOT_PANEL::GetUnitsY3() const
{
    LIN_SCALE<mpScaleY>* linScale = dynamic_cast<LIN_SCALE<mpScaleY>*>( m_axis_y3 );

    if( linScale )
        return linScale->GetUnits();
    else
        return wxEmptyString;
}


void SIM_PLOT_PANEL::updateAxes( int aNewTraceType )
{
    switch( GetType() )
    {
        case ST_AC:
            if( !m_axis_x )
            {
                m_axis_x = new LOG_SCALE<mpScaleXLog>( wxEmptyString, wxT( "Hz" ), mpALIGN_BOTTOM );
                m_axis_x->SetNameAlign( mpALIGN_BOTTOM );
                m_plotWin->AddLayer( m_axis_x );

                m_axis_y1 = new LIN_SCALE<mpScaleY>( wxEmptyString, wxT( "dBV" ), mpALIGN_LEFT );
                m_axis_y1->SetNameAlign( mpALIGN_LEFT );
                m_plotWin->AddLayer( m_axis_y1 );

                m_axis_y2 = new LIN_SCALE<mpScaleY>( wxEmptyString, wxT( "°" ), mpALIGN_RIGHT );
                m_axis_y2->SetNameAlign( mpALIGN_RIGHT );
                m_axis_y2->SetMasterScale( m_axis_y1 );
                m_plotWin->AddLayer( m_axis_y2 );
            }

            m_axis_x->SetName( _( "Frequency" ) );
            m_axis_y1->SetName( _( "Gain" ) );
            m_axis_y2->SetName( _( "Phase" ) );
            break;

        case ST_DC:
            prepareDCAxes( aNewTraceType );
            break;

        case ST_NOISE:
            if( !m_axis_x )
            {
                m_axis_x = new LOG_SCALE<mpScaleXLog>( wxEmptyString, wxT( "Hz" ), mpALIGN_BOTTOM );
                m_axis_x->SetNameAlign( mpALIGN_BOTTOM );
                m_plotWin->AddLayer( m_axis_x );

                m_axis_y1 = new mpScaleY( wxEmptyString, mpALIGN_LEFT, false );
                m_axis_y1->SetNameAlign( mpALIGN_LEFT );
                m_plotWin->AddLayer( m_axis_y1 );
            }

            m_axis_x->SetName( _( "Frequency" ) );
            m_axis_y1->SetName( _( "noise [(V or A)^2/Hz]" ) );
            break;

        case ST_TRANSIENT:
            if( !m_axis_x )
            {
                m_axis_x = new LIN_SCALE<mpScaleX>( wxEmptyString, wxT( "s" ), mpALIGN_BOTTOM );
                m_axis_x->SetNameAlign( mpALIGN_BOTTOM );
                m_plotWin->AddLayer( m_axis_x );

                m_axis_y1 = new LIN_SCALE<mpScaleY>(wxEmptyString, wxT( "V" ), mpALIGN_LEFT );
                m_axis_y1->SetNameAlign( mpALIGN_LEFT );
                m_plotWin->AddLayer( m_axis_y1 );

                m_axis_y2 = new LIN_SCALE<mpScaleY>( wxEmptyString, wxT( "A" ), mpALIGN_RIGHT );
                m_axis_y2->SetNameAlign( mpALIGN_RIGHT );
                m_axis_y2->SetMasterScale( m_axis_y1 );
                m_plotWin->AddLayer( m_axis_y2 );
            }

            m_axis_x->SetName( _( "Time" ) );
            m_axis_y1->SetName( _( "Voltage" ) );
            m_axis_y2->SetName( _( "Current" ) );

            if( ( aNewTraceType & SPT_POWER ) && !m_axis_y3 )
            {
                m_plotWin->SetMargins( 35, 140, 35, 70 );

                m_axis_y3 = new LIN_SCALE<mpScaleY>( wxEmptyString, wxT( "W" ), mpALIGN_FAR_RIGHT );
                m_axis_y3->SetNameAlign( mpALIGN_FAR_RIGHT );
                m_axis_y3->SetMasterScale( m_axis_y1 );
                m_plotWin->AddLayer( m_axis_y3 );
            }

            if( m_axis_y3 )
                m_axis_y3->SetName( _( "Power" ) );

            break;

        default:
            // suppress warnings
            break;
    }
}

void SIM_PLOT_PANEL::prepareDCAxes( int aNewTraceType )
{
    wxString sim_cmd = GetSimCommand().Lower();
    wxString rem;

    if( sim_cmd.StartsWith( ".dc", &rem ) )
    {
        wxChar ch = 0;

        rem.Trim( false );

        try
        {
            ch = rem.GetChar( 0 );
        }
        catch( ... )
        {
            // Best efforts
        }

        switch( ch )
        {
        // Make sure that we have a reliable default (even if incorrectly labeled)
        default:
        case 'v':
            if( !m_axis_x )
            {
                m_axis_x = new LIN_SCALE<mpScaleX>( wxEmptyString, wxT( "V" ), mpALIGN_BOTTOM );
                m_axis_x->SetNameAlign( mpALIGN_BOTTOM );
                m_plotWin->AddLayer( m_axis_x );
            }

            m_axis_x->SetName( _( "Voltage (swept)" ) );
            break;

        case 'i':
            if( !m_axis_x )
            {
                m_axis_x = new LIN_SCALE<mpScaleX>( wxEmptyString, wxT( "A" ), mpALIGN_BOTTOM );
                m_axis_x->SetNameAlign( mpALIGN_BOTTOM );
                m_plotWin->AddLayer( m_axis_x );
            }

            m_axis_x->SetName( _( "Current (swept)" ) );
            break;

        case 'r':
            if( !m_axis_x )
            {
                m_axis_x = new LIN_SCALE<mpScaleX>( wxEmptyString, wxT( "Ω" ), mpALIGN_BOTTOM );
                m_axis_x->SetNameAlign( mpALIGN_BOTTOM );
                m_plotWin->AddLayer( m_axis_x );
            }

            m_axis_x->SetName( _( "Resistance (swept)" ) );
            break;

        case 't':
            if( !m_axis_x )
            {
                m_axis_x = new LIN_SCALE<mpScaleX>( wxEmptyString, wxT( "°C" ), mpALIGN_BOTTOM );
                m_axis_x->SetNameAlign( mpALIGN_BOTTOM );
                m_plotWin->AddLayer( m_axis_x );
            }

            m_axis_x->SetName( _( "Temperature (swept)" ) );
            break;
        }

        if( !m_axis_y1 )
        {
            m_axis_y1 = new LIN_SCALE<mpScaleY>( wxEmptyString, wxT( "V" ), mpALIGN_LEFT );
            m_axis_y1->SetNameAlign( mpALIGN_LEFT );
            m_plotWin->AddLayer( m_axis_y1 );
        }

        if( !m_axis_y2 )
        {
            m_axis_y2 = new LIN_SCALE<mpScaleY>( wxEmptyString, wxT( "A" ), mpALIGN_RIGHT );
            m_axis_y2->SetNameAlign( mpALIGN_RIGHT );
            m_plotWin->AddLayer( m_axis_y2 );
        }

        m_axis_y1->SetName( _( "Voltage (measured)" ) );
        m_axis_y2->SetName( _( "Current" ) );

        if( ( aNewTraceType & SPT_POWER ) && !m_axis_y3 )
        {
            m_plotWin->SetMargins( 35, 140, 35, 70 );

            m_axis_y3 = new LIN_SCALE<mpScaleY>( wxEmptyString, wxT( "W" ), mpALIGN_FAR_RIGHT );
            m_axis_y3->SetNameAlign( mpALIGN_FAR_RIGHT );
            m_axis_y3->SetMasterScale( m_axis_y1 );
            m_plotWin->AddLayer( m_axis_y3 );
        }

        if( m_axis_y3 )
            m_axis_y3->SetName( _( "Power" ) );
    }
}


void SIM_PLOT_PANEL::UpdatePlotColors()
{
    // Update bg and fg colors:
    m_plotWin->SetColourTheme( m_colors.GetPlotColor( SIM_PLOT_COLORS::COLOR_SET::BACKGROUND ),
                               m_colors.GetPlotColor( SIM_PLOT_COLORS::COLOR_SET::FOREGROUND ),
                               m_colors.GetPlotColor( SIM_PLOT_COLORS::COLOR_SET::AXIS ) );

    // Update color of all traces
    for( const auto& [ name, trace ] : m_traces )
    {
        for( const auto& [ id, cursor ] : trace->GetCursors() )
        {
            if( cursor )
                cursor->SetPen( wxPen( m_colors.GetPlotColor( SIM_PLOT_COLORS::COLOR_SET::CURSOR ) ) );
        }
    }

    m_plotWin->UpdateAll();
}


void SIM_PLOT_PANEL::OnLanguageChanged()
{
    updateAxes();
    m_plotWin->UpdateAll();
}


void SIM_PLOT_PANEL::UpdateTraceStyle( TRACE* trace )
{
    int        type = trace->GetType();
    wxPenStyle penStyle = ( ( ( type & SPT_AC_PHASE ) || ( type & SPT_CURRENT ) ) && m_dotted_cp )
                                  ? wxPENSTYLE_DOT
                                  : wxPENSTYLE_SOLID;
    trace->SetPen( wxPen( trace->GetTraceColour(), 2, penStyle ) );
}


TRACE* SIM_PLOT_PANEL::AddTrace( const wxString& aVectorName, int aType )
{
    TRACE* trace = GetTrace( aVectorName, aType );

    if( !trace )
    {
        updateAxes( aType );

        if( GetType() == ST_TRANSIENT || GetType() == ST_DC )
        {
            bool hasVoltageTraces = false;

            for( const auto& [ id, candidate ] : m_traces )
            {
                if( candidate->GetType() & SPT_VOLTAGE )
                {
                    hasVoltageTraces = true;
                    break;
                }
            }

            if( !hasVoltageTraces )
            {
                if( m_axis_y2 )
                    m_axis_y2->SetMasterScale( nullptr );

                if( m_axis_y3 )
                    m_axis_y3->SetMasterScale( nullptr );
            }
        }

        trace = new TRACE( aVectorName, (SIM_TRACE_TYPE) aType );
        trace->SetTraceColour( m_colors.GenerateColor( m_traces ) );
        UpdateTraceStyle( trace );
        m_traces[ getTraceId( aVectorName, aType ) ] = trace;

        m_plotWin->AddLayer( (mpLayer*) trace );
    }

    return trace;
}


void SIM_PLOT_PANEL::SetTraceData( TRACE* trace, unsigned int aPoints, const double* aX,
                                   const double* aY )
{
    std::vector<double> tmp( aY, aY + aPoints );

    if( GetType() == ST_AC )
    {
        if( trace->GetType() & SPT_AC_PHASE )
        {
            for( unsigned int i = 0; i < aPoints; i++ )
                tmp[i] = tmp[i] * 180.0 / M_PI;                 // convert to degrees
        }
        else
        {
            for( unsigned int i = 0; i < aPoints; i++ )
            {
                // log( 0 ) is not valid.
                if( tmp[i] != 0 )
                    tmp[i] = 20 * log( tmp[i] ) / log( 10.0 );  // convert to dB
            }
        }
    }

    trace->SetData( std::vector<double>( aX, aX + aPoints ), tmp );

    if( ( trace->GetType() & SPT_AC_PHASE ) || ( trace->GetType() & SPT_CURRENT ) )
        trace->SetScale( m_axis_x, m_axis_y2 );
    else if( trace->GetType() & SPT_POWER )
        trace->SetScale( m_axis_x, m_axis_y3 );
    else
        trace->SetScale( m_axis_x, m_axis_y1 );

    for( auto& [ cursorId, cursor ] : trace->GetCursors() )
    {
        if( cursor )
            cursor->SetCoordX( cursor->GetCoords().x );
    }

    m_plotWin->UpdateAll();
}


void SIM_PLOT_PANEL::DeleteTrace( TRACE* aTrace )
{
    for( const auto& [ name, trace ] : m_traces )
    {
        if( trace == aTrace )
        {
            m_traces.erase( name );
            break;
        }
    }

    for( const auto& [ id, cursor ] : aTrace->GetCursors() )
    {
        if( cursor )
            m_plotWin->DelLayer( cursor, true );
    }

    m_plotWin->DelLayer( aTrace, true, true );
    ResetScales();
}


bool SIM_PLOT_PANEL::DeleteTrace( const wxString& aVectorName, int aTraceType )
{
    if( TRACE* trace = GetTrace( aVectorName, aTraceType ) )
    {
        DeleteTrace( trace );
        return true;
    }

    return false;
}


void SIM_PLOT_PANEL::EnableCursor( const wxString& aVectorName, int aType, int aCursorId,
                                   bool aEnable, const wxString& aSignalName )
{
    TRACE* t = GetTrace( aVectorName, aType );

    if( t == nullptr || t->HasCursor( aCursorId ) == aEnable )
        return;

    if( aEnable )
    {
        CURSOR*   cursor = new CURSOR( t, this );
        mpWindow* win = GetPlotWin();
        int       width = win->GetXScreen() - win->GetMarginLeft() - win->GetMarginRight();
        int       center = win->GetMarginLeft() + KiROUND( width * ( aCursorId == 1 ? 0.4 : 0.6 ) );

        cursor->SetName( aSignalName );
        cursor->SetX( center );
        cursor->SetPen( wxPen( m_colors.GetPlotColor( SIM_PLOT_COLORS::COLOR_SET::CURSOR ) ) );

        t->SetCursor( aCursorId, cursor );
        m_plotWin->AddLayer( cursor );
    }
    else
    {
        CURSOR* cursor = t->GetCursor( aCursorId );
        t->SetCursor( aCursorId, nullptr );
        m_plotWin->DelLayer( cursor, true );
    }

    // Notify the parent window about the changes
    wxQueueEvent( GetParent(), new wxCommandEvent( EVT_SIM_CURSOR_UPDATE ) );
}


void SIM_PLOT_PANEL::ResetScales()
{
    if( m_axis_x )
        m_axis_x->ResetDataRange();

    if( m_axis_y1 )
        m_axis_y1->ResetDataRange();

    if( m_axis_y2 )
        m_axis_y2->ResetDataRange();

    if( m_axis_y3 )
        m_axis_y3->ResetDataRange();

    for( auto& [ name, trace ] : m_traces )
        trace->UpdateScales();
}


wxDEFINE_EVENT( EVT_SIM_CURSOR_UPDATE, wxCommandEvent );
