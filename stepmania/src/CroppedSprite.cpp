#include "stdafx.h"
/*
-----------------------------------------------------------------------------
 File: CroppedSprite.h

 Desc: The song's CroppedSprite displayed in SelectSong.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
-----------------------------------------------------------------------------
*/

#include "RageUtil.h"

#include "CroppedSprite.h"
#include "ThemeManager.h"
#include "RageBitmapTexture.h"


void CroppedSprite::CropToSize( float fWidth, float fHeight )
{
	m_fCropWidth = fWidth;
	m_fCropHeight = fHeight;


	int iSourceWidth	= m_pTexture->GetSourceWidth();
	int iSourceHeight	= m_pTexture->GetSourceHeight();

	// save the original X&Y.  We're going to resore them later.
	float fOriginalX = GetX();
	float fOriginalY = GetY();

	if( iSourceWidth == iSourceHeight )		// this is a SSR/DWI CroppedSprite
	{
		float fCustomImageCoords[8] = {
			0.22f,	0.98f,	// bottom left
			0.02f,	0.78f,	// top left
			0.98f,	0.22f,	// bottom right
			0.78f,	0.02f,	// top right
		};
		Sprite::SetCustomImageCoords( fCustomImageCoords );
		
		SetWidth(  m_fCropWidth );
		SetHeight( m_fCropHeight );
		SetZoom( 1 );
	}
	else	// this is probably a background graphic or something not intended to be a CroppedSprite
	{
		Sprite::StopUsingCustomCoords();

		// first find the correct zoom
		Sprite::ScaleToCover( CRect(0, 0,
									(int)m_fCropWidth,
									(int)m_fCropHeight )
							 );
		float fFinalZoom = this->GetZoom();
		
		// find which dimension is larger
		bool bXDimNeedsToBeCropped = GetZoomedWidth() > m_fCropWidth;
		
		if( bXDimNeedsToBeCropped )	// crop X
		{
			float fPercentageToCutOff = (this->GetZoomedWidth() - m_fCropWidth) / this->GetZoomedWidth();
			float fPercentageToCutOffEachSide = fPercentageToCutOff / 2;
			
			// generate a rectangle with new texture coordinates
			FRECT fCustomImageCoords( 
				fPercentageToCutOffEachSide, 
				0, 
				1 - fPercentageToCutOffEachSide, 
				1 );
			SetCustomImageRect( fCustomImageCoords );
		}
		else		// crop Y
		{
			float fPercentageToCutOff = (this->GetZoomedHeight() - m_fCropHeight) / this->GetZoomedHeight();
			float fPercentageToCutOffEachSide = fPercentageToCutOff / 2;
			
			// generate a rectangle with new texture coordinates
			FRECT fCustomImageCoords( 
				0, 
				fPercentageToCutOffEachSide,
				1, 
				1 - fPercentageToCutOffEachSide );
			SetCustomImageRect( fCustomImageCoords );
		}
		SetWidth(  m_fCropWidth );
		SetHeight( m_fCropHeight );
		SetZoom( 1 );
	}

	// restore original XY
	SetXY( fOriginalX, fOriginalY );
}
