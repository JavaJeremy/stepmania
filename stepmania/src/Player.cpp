#include "stdafx.h"
/*
-----------------------------------------------------------------------------
 Class: Notes

 Desc: See header.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
-----------------------------------------------------------------------------
*/

#include "GameConstantsAndTypes.h"
#include "Math.h" // for fabs()
#include "Player.h"
#include "RageUtil.h"
#include "ThemeManager.h"
#include "GameConstantsAndTypes.h"
#include "ArrowEffects.h"
#include "GameManager.h"
#include "InputMapper.h"


// these two items are in the
const float FRAME_JUDGE_AND_COMBO_Y = CENTER_Y;
const float JUDGEMENT_Y_OFFSET	= -26;
const float COMBO_Y_OFFSET		= +26;

const float ARROWS_Y			= SCREEN_TOP + ARROW_SIZE * 1.5f;
const float HOLD_JUDGEMENT_Y	= ARROWS_Y + 80;

const float HOLD_ARROW_NG_TIME	=	0.27f;


Player::Player()
{

	m_fSongBeat = 0;
	m_PlayerNumber = PLAYER_NONE;

	// init step elements
	for( int i=0; i<MAX_TAP_NOTE_ROWS; i++ )
	{
		for( int c=0; c<MAX_NOTE_TRACKS; c++ )
		{
			m_TapNotesOriginal[c][i] = '0';
			m_TapNotes[c][i] = '0';
		}
		m_TapNoteScores[i] = TNS_NONE;
	}
	m_iNumHoldNotes = 0;

	m_pLifeMeter = NULL;
	m_pScore = NULL;

	this->AddActor( &m_GrayArrowRow );
	this->AddActor( &m_NoteField );
	this->AddActor( &m_GhostArrowRow );

	m_frameJudgeAndCombo.AddActor( &m_Judgement );
	m_frameJudgeAndCombo.AddActor( &m_Combo );
	this->AddActor( &m_frameJudgeAndCombo );
	
	for( int c=0; c<MAX_NOTE_TRACKS; c++ )
		this->AddActor( &m_HoldJudgement[c] );
}


void Player::Load( PlayerNumber player_no, NoteData* pNoteData, const PlayerOptions& po, LifeMeterBar* pLM, ScoreDisplayRolling* pScore )
{
	//LOG->WriteLine( "Player::Load()", );
	this->CopyAll( pNoteData );

	m_PlayerNumber = player_no;
	m_PlayerOptions = po;

	m_pLifeMeter = pLM;
	m_pScore = pScore;

	if( !po.m_bAllowFreezeArrows )
		this->RemoveHoldNotes();

	this->Turn( po.m_TurnType );

	if( po.m_bLittle )
		this->MakeLittle();

	m_NoteField.Load( (NoteData*)this, player_no, po, 1.5f, 5.5f, NoteField::MODE_DANCING );
	
	for( int t=0; t<pNoteData->m_iNumTracks; t++ )
	{
		for( int r=0; r<MAX_TAP_NOTE_ROWS; r++ )
			m_TapNotesOriginal[t][r] = pNoteData->m_TapNotes[t][r];
	}

	m_GrayArrowRow.Load( po );
	m_GhostArrowRow.Load( po );

	m_frameJudgeAndCombo.SetY( FRAME_JUDGE_AND_COMBO_Y );
	m_Combo.SetY( po.m_bReverseScroll ?  -COMBO_Y_OFFSET : COMBO_Y_OFFSET );
	m_Judgement.SetY( po.m_bReverseScroll ? -JUDGEMENT_Y_OFFSET : JUDGEMENT_Y_OFFSET );

	for( int c=0; c<MAX_NOTE_TRACKS; c++ )
		m_HoldJudgement[c].SetY( po.m_bReverseScroll ? SCREEN_HEIGHT - HOLD_JUDGEMENT_Y : HOLD_JUDGEMENT_Y );

	m_NoteField.SetY( po.m_bReverseScroll ? SCREEN_HEIGHT - ARROWS_Y : ARROWS_Y );
	m_GrayArrowRow.SetY( po.m_bReverseScroll ? SCREEN_HEIGHT - ARROWS_Y : ARROWS_Y );
	m_GhostArrowRow.SetY( po.m_bReverseScroll ? SCREEN_HEIGHT - ARROWS_Y : ARROWS_Y );
}

void Player::Update( float fDeltaTime, float fSongBeat, float fMaxBeatDifference )
{
	//LOG->WriteLine( "Player::Update(%f, %f, %f)", fDeltaTime, fSongBeat, fMaxBeatDifference );


	//
	// Check for TapNote misses
	//
	int iNumMisses = UpdateTapNotesMissedOlderThan( m_fSongBeat-fMaxBeatDifference );
	if( iNumMisses > 0 )
	{
		m_Judgement.SetJudgement( TNS_MISS );
		m_Combo.EndCombo();
		for( int i=0; i<iNumMisses; i++ )
			m_pLifeMeter->ChangeLife( TNS_MISS );
	}


	//
	// update HoldNotes logic
	//
	for( int i=0; i<m_iNumHoldNotes; i++ )		// for each HoldNote
	{
		HoldNote &hn = m_HoldNotes[i];
		HoldNoteScore &hns = m_HoldNoteScores[i];

		if( hns.m_Result != HNR_NONE )	// if this HoldNote already has a result
			continue;	// we don't need to update the logic for this one

		float fStartBeat = NoteRowToBeat( (float)hn.m_iStartIndex );
		float fEndBeat = NoteRowToBeat( (float)hn.m_iEndIndex );

		const int iCol = hn.m_iTrack;
		const StyleInput StyleI( m_PlayerNumber, hn.m_iTrack );
		const GameInput GameI = GAME->GetCurrentStyleDef()->StyleInputToGameInput( StyleI );

		// update the life
		if( fStartBeat < m_fSongBeat && m_fSongBeat < fEndBeat )	// if the song beat is in the range of this hold
		{
			const bool bIsHoldingButton = INPUTMAPPER->IsButtonDown( GameI );

			if( bIsHoldingButton )
			{
				hns.m_fLife += fDeltaTime/HOLD_ARROW_NG_TIME;
				hns.m_fLife = min( hns.m_fLife, 1 );	// clamp
				m_GhostArrowRow.HoldNote( iCol );		// update the "electric ghost" effect
			}
			else	// !bIsHoldingButton
			{
				hns.m_fLife -= fDeltaTime/HOLD_ARROW_NG_TIME;
				hns.m_fLife = max( hns.m_fLife, 0 );	// clamp
			}
			m_NoteField.SetHoldNoteLife( i, hns.m_fLife );	// update the NoteField display
		}

		// check for NG
		if( hns.m_fLife == 0 )	// the player has not pressed the button for a long time!
		{
			hns.m_Result = HNR_NG;
			m_HoldJudgement[iCol].SetHoldJudgement( HNR_NG );
		}

		// check for OK
		if( m_fSongBeat > fEndBeat )	// if this HoldNote is in the past
		{
			// this implies that hns.m_fLife > 0, or else we would have marked it NG above
			hns.m_fLife = 1;
			hns.m_Result = HNR_OK;
			m_HoldJudgement[iCol].SetHoldJudgement( HNR_OK );
			m_NoteField.SetHoldNoteLife( i, hns.m_fLife );	// update the NoteField display
		}
	}



	ActorFrame::Update( fDeltaTime );

	m_frameJudgeAndCombo.Update( fDeltaTime );

	m_pLifeMeter->SetBeat( fSongBeat );

	m_GrayArrowRow.Update( fDeltaTime, fSongBeat );
	m_NoteField.Update( fDeltaTime, fSongBeat );
	m_GhostArrowRow.Update( fDeltaTime, fSongBeat );

	m_fSongBeat = fSongBeat;	// save song beat

}

void Player::DrawPrimitives()
{
	D3DXMATRIX matOldView, matOldProj;

	if( m_PlayerOptions.m_EffectType == PlayerOptions::EFFECT_SPACE )
	{
		// turn off Z Buffering
		DISPLAY->GetDevice()->SetRenderState( D3DRS_ZENABLE,      FALSE );
		DISPLAY->GetDevice()->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

		// save old view and projection
		DISPLAY->GetDevice()->GetTransform( D3DTS_VIEW, &matOldView );
		DISPLAY->GetDevice()->GetTransform( D3DTS_PROJECTION, &matOldProj );


		// construct view and project matrix
		D3DXMATRIX matNewView;
		D3DXMatrixLookAtLH( &matNewView, &D3DXVECTOR3( CENTER_X, GetY()+800.0f, 300.0f ), 
										 &D3DXVECTOR3( CENTER_X
										 , GetY()+400.0f,   0.0f ), 
										 &D3DXVECTOR3(          0.0f,         -1.0f,   0.0f ) );
		DISPLAY->GetDevice()->SetTransform( D3DTS_VIEW, &matNewView );

		D3DXMATRIX matNewProj;
		D3DXMatrixPerspectiveFovLH( &matNewProj, D3DX_PI/4.0f, SCREEN_WIDTH/(float)SCREEN_HEIGHT, 0.0f, 1000.0f );
		DISPLAY->GetDevice()->SetTransform( D3DTS_PROJECTION, &matNewProj );
	}

	m_GrayArrowRow.Draw();
	m_NoteField.Draw();
	m_GhostArrowRow.Draw();

	if( m_PlayerOptions.m_EffectType == PlayerOptions::EFFECT_SPACE )
	{
		// restire old view and projection
		DISPLAY->GetDevice()->SetTransform( D3DTS_VIEW, &matOldView );
		DISPLAY->GetDevice()->SetTransform( D3DTS_PROJECTION, &matOldProj );

		// turn Z Buffering back on
		DISPLAY->GetDevice()->SetRenderState( D3DRS_ZENABLE,      TRUE );
		DISPLAY->GetDevice()->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
	}

	m_frameJudgeAndCombo.Draw();

	for( int c=0; c<m_iNumTracks; c++ )
		m_HoldJudgement[c].Draw();
}

bool Player::IsThereANoteAtIndex( int iIndex )
{
	for( int t=0; t<m_iNumTracks; t++ )
	{
		if( m_TapNotesOriginal[t][iIndex] != '0' )
			return true;
	}

	for( int i=0; i<m_iNumHoldNotes; i++ )	// for each HoldNote
	{
		if( m_HoldNotes[i].m_iStartIndex == iIndex )
			return true;
	}

	return false;
}




void Player::HandlePlayerStep( float fSongBeat, ColumnNumber col, float fMaxBeatDiff )
{
	//LOG->WriteLine( "Player::HandlePlayerStep()" );

	ASSERT( col >= 0  &&  col <= m_iNumTracks );

	m_GrayArrowRow.Step( col );

	CheckForCompleteRow( fSongBeat, col, fMaxBeatDiff );

	//
	// check if we stepped on the TapNote part of a HoldNote
	//
	for( int i=0; i<m_iNumHoldNotes; i++ )	// for each HoldNote
	{
		HoldNote& hn		= m_HoldNotes[i];
		HoldNoteScore& hns	= m_HoldNoteScores[i];

		if( hns.m_Result != HNR_NONE )	// if this note already has a score
			continue;	// we don't need to update its logic

		if( hns.m_TapNoteScore != TNS_NONE )	// the TapNote already has a score
			continue;	// no need to continue;
			
		if( col == m_HoldNotes[i].m_iTrack ) // the player's step is the same as this HoldNote
		{
			float fBeatDifference = fabsf( NoteRowToBeat(hn.m_iStartIndex) - fSongBeat );
			
			if( fBeatDifference <= fMaxBeatDiff )
			{
				float fBeatsUntilStep = NoteRowToBeat( (float)hn.m_iStartIndex ) - fSongBeat;
				float fPercentFromPerfect = fabsf( fBeatsUntilStep / fMaxBeatDiff );

				//LOG->WriteLine( "fBeatsUntilStep: %f, fPercentFromPerfect: %f", 
				//		 fBeatsUntilStep, fPercentFromPerfect );

				// compute what the score should be for the note we stepped on
				TapNoteScore &score = hns.m_TapNoteScore;
				if(		 fPercentFromPerfect < 0.25f )	score = TNS_PERFECT;
				else if( fPercentFromPerfect < 0.50f )	score = TNS_GREAT;
				else if( fPercentFromPerfect < 0.75f )	score = TNS_GOOD;
				else									score = TNS_BOO;

				// update the judgement, score, and life
				m_Judgement.SetJudgement( score );
				m_pScore->AddToScore( score, m_Combo.GetCurrentCombo() );
				m_pLifeMeter->ChangeLife( score );

				// show the gray arrow ghost
				m_GhostArrowRow.TapNote( col, score, m_Combo.GetCurrentCombo() > 100 );

				// update the combo display
				switch( score )
				{
				case TNS_PERFECT:
				case TNS_GREAT:
					m_Combo.ContinueCombo();
					break;
				case TNS_GOOD:
				case TNS_BOO:
					m_Combo.EndCombo();
					break;
				}

				// zoom the judgement and combo like a heart beat
				float fStartZoom;
				switch( score )
				{
				case TNS_PERFECT:	fStartZoom = 1.5f;	break;
				case TNS_GREAT:		fStartZoom = 1.3f;	break;
				case TNS_GOOD:		fStartZoom = 1.2f;	break;
				case TNS_BOO:		fStartZoom = 1.0f;	break;
				}
				m_frameJudgeAndCombo.SetZoom( fStartZoom );
				m_frameJudgeAndCombo.BeginTweening( 0.2f );
				m_frameJudgeAndCombo.SetTweenZoom( 1 );
			}
		}
	}

}


void Player::CheckForCompleteRow( float fSongBeat, ColumnNumber col, float fMaxBeatDiff )
{
	//LOG->WriteLine( "Player::CheckForCompleteRow()" );

	// look for the closest matching step
	int iIndexStartLookingAt = BeatToNoteRow( fSongBeat );
	int iNumElementsToExamine = BeatToNoteRow( fMaxBeatDiff );	// number of elements to examine on either end of iIndexStartLookingAt
	
	//LOG->WriteLine( "iIndexStartLookingAt = %d, iNumElementsToExamine = %d", iIndexStartLookingAt, iNumElementsToExamine );

	// Start at iIndexStartLookingAt and search outward.  The first one that overlaps the player's step is the closest match.
	for( int delta=0; delta <= iNumElementsToExamine; delta++ )
	{
		int iCurrentIndexEarlier = iIndexStartLookingAt - delta;
		int iCurrentIndexLater   = iIndexStartLookingAt + delta;

		// silly check to make sure we don't go out of bounds
		iCurrentIndexEarlier	= clamp( iCurrentIndexEarlier, 0, MAX_TAP_NOTE_ROWS-1 );
		iCurrentIndexLater		= clamp( iCurrentIndexLater,   0, MAX_TAP_NOTE_ROWS-1 );

		////////////////////////////
		// check the step to the left of iIndexStartLookingAt
		////////////////////////////
		//LOG->WriteLine( "Checking Notes[%d]", iCurrentIndexEarlier );
		if( m_TapNotes[col][iCurrentIndexEarlier] != '0' )	// these Notes overlap
		{
			m_TapNotes[col][iCurrentIndexEarlier] = '0';	// mark hit
			
			bool bRowDestroyed = true;
			for( int t=0; t<m_iNumTracks; t++ )			// did this complete the elminiation of the row?
			{
				if( m_TapNotes[col][iCurrentIndexEarlier] != '0' )
					bRowDestroyed = false;
			}
			if( bRowDestroyed )
				OnRowDestroyed( fSongBeat, col, fMaxBeatDiff, iCurrentIndexEarlier );
			return;
		}

		////////////////////////////
		// check the step to the right of iIndexStartLookingAt
		////////////////////////////
		//LOG->WriteLine( "Checking Notes[%d]", iCurrentIndexLater );
		if( m_TapNotes[col][iCurrentIndexLater] != '0' )	// these Notes overlap
		{
			m_TapNotes[col][iCurrentIndexLater] = '0';	// mark hit
			
			bool bRowDestroyed = true;
			for( int t=0; t<m_iNumTracks; t++ )			// did this complete the elminiation of the row?
			{
				if( m_TapNotes[col][iCurrentIndexLater] != '0' )
					bRowDestroyed = false;
			}
			if( bRowDestroyed )
				OnRowDestroyed( fSongBeat, col, fMaxBeatDiff, iCurrentIndexLater );
			return;
		}
	}
}

void Player::OnRowDestroyed( float fSongBeat, ColumnNumber col, float fMaxBeatDiff, int iIndexThatWasSteppedOn )
{
	float fStepBeat = NoteRowToBeat( (float)iIndexThatWasSteppedOn );

 
	float fBeatsUntilStep = fStepBeat - fSongBeat;
	float fPercentFromPerfect = fabsf( fBeatsUntilStep / fMaxBeatDiff );

	//LOG->WriteLine( "fBeatsUntilStep: %f, fPercentFromPerfect: %f", 
	//		 fBeatsUntilStep, fPercentFromPerfect );

	// compute what the score should be for the note we stepped on
	TapNoteScore &score = m_TapNoteScores[iIndexThatWasSteppedOn];

	if(		 fPercentFromPerfect < 0.25f )	score = TNS_PERFECT;
	else if( fPercentFromPerfect < 0.50f )	score = TNS_GREAT;
	else if( fPercentFromPerfect < 0.75f )	score = TNS_GOOD;
	else									score = TNS_BOO;

	// update the judgement, score, and life
	m_Judgement.SetJudgement( score );
	m_pScore->AddToScore( score, m_Combo.GetCurrentCombo() );
	m_pLifeMeter->ChangeLife( score );


	// remove this row from the NoteField
	if ( ( score == TNS_PERFECT ) || ( score == TNS_GREAT ) )
		m_NoteField.RemoveTapNoteRow( iIndexThatWasSteppedOn );

	// show ghost arrows
	for( int c=0; c<m_iNumTracks; c++ )	// for each column
	{
		if( m_TapNotesOriginal[c][iIndexThatWasSteppedOn] != '0' )	// if there is an original note note in this column
			m_GhostArrowRow.TapNote( c, score, m_Combo.GetCurrentCombo()>100 );	// show the ghost arrow for this column
	}

	// update the combo display
	switch( score )
	{
	case TNS_PERFECT:
	case TNS_GREAT:
		m_Combo.ContinueCombo();
		break;
	case TNS_GOOD:
	case TNS_BOO:
		m_Combo.EndCombo();
		break;
	}

	// zoom the judgement and combo like a heart beat
	float fStartZoom;
	switch( score )
	{
	case TNS_PERFECT:	fStartZoom = 1.5f;	break;
	case TNS_GREAT:		fStartZoom = 1.3f;	break;
	case TNS_GOOD:		fStartZoom = 1.2f;	break;
	case TNS_BOO:		fStartZoom = 1.0f;	break;
	}
	m_frameJudgeAndCombo.SetZoom( fStartZoom );
	m_frameJudgeAndCombo.BeginTweening( 0.2f );
	m_frameJudgeAndCombo.SetTweenZoom( 1 );
}


ScoreSummary Player::GetScoreSummary()
{
	ScoreSummary scoreSummary;

	for( int i=0; i<MAX_TAP_NOTE_ROWS; i++ ) 
	{
		switch( m_TapNoteScores[i] )
		{
		case TNS_PERFECT:	scoreSummary.perfect++;		break;
		case TNS_GREAT:		scoreSummary.great++;		break;
		case TNS_GOOD:		scoreSummary.good++;		break;
		case TNS_BOO:		scoreSummary.boo++;			break;
		case TNS_MISS:		scoreSummary.miss++;		break;
		case TNS_NONE:									break;
		default:		ASSERT( false );
		}
	}
	for( i=0; i<m_iNumHoldNotes; i++ ) 
	{
		switch( m_HoldNoteScores[i].m_TapNoteScore )
		{
		case TNS_PERFECT:	scoreSummary.perfect++;		break;
		case TNS_GREAT:		scoreSummary.great++;		break;
		case TNS_GOOD:		scoreSummary.good++;		break;
		case TNS_BOO:		scoreSummary.boo++;			break;
		case TNS_MISS:		scoreSummary.miss++;		break;
		case TNS_NONE:									break;
		default:		ASSERT( false );
		}
		switch( m_HoldNoteScores[i].m_Result )
		{
		case HNR_NG:		scoreSummary.ng++;		break;
		case HNR_OK:		scoreSummary.ok++;		break;
		case HNR_NONE:								break;
		default:		ASSERT( false );
		}
	}
	scoreSummary.max_combo = m_Combo.GetMaxCombo();
	scoreSummary.score = m_pScore ? m_pScore->GetScore() : 0;
	
	return scoreSummary;
}


int Player::UpdateTapNotesMissedOlderThan( float fMissIfOlderThanThisBeat )
{
	//LOG->WriteLine( "Notes::UpdateTapNotesMissedOlderThan(%f)", fMissIfOlderThanThisBeat );

	int iMissIfOlderThanThisIndex = BeatToNoteRow( fMissIfOlderThanThisBeat );

	int iNumMissesFound = 0;
	// Since this is being called every frame, let's not check the whole array every time.
	// Instead, only check 10 elements back.  Even 10 is overkill.
	int iStartCheckingAt = max( 0, iMissIfOlderThanThisIndex-10 );

	//LOG->WriteLine( "iStartCheckingAt: %d   iMissIfOlderThanThisIndex:  %d", iStartCheckingAt, iMissIfOlderThanThisIndex );
	for( int i=iStartCheckingAt; i<iMissIfOlderThanThisIndex; i++ )
	{
		if( m_TapNoteScores[i] != TNS_NONE )	// this index already has a score
			continue;

		// look for any track at this index that still has data
		if( !this->IsRowEmpty( i ) )
		{
			m_TapNoteScores[i] = TNS_MISS;
			iNumMissesFound++;
			m_pLifeMeter->ChangeLife( TNS_MISS );
		}
	}

	return iNumMissesFound;
}







