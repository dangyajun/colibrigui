
#include "CrystalGui/CrystalRenderable.h"

#include "CrystalGui/CrystalWindow.h"
#include "CrystalGui/CrystalManager.h"
#include "CrystalGui/Ogre/CrystalOgreRenderable.h"

#include "OgreBitwise.h"

#define TODO_borderRepeatSize
#define TODO_this_is_a_workaround_neg_y

CRYSTALGUI_ASSUME_NONNULL_BEGIN

namespace Crystal
{
	//-------------------------------------------------------------------------
	inline void Renderable::addQuad( UiVertex * RESTRICT_ALIAS vertexBuffer,
	                                 Ogre::Vector2 topLeft,
	                                 Ogre::Vector2 bottomRight,
	                                 Ogre::Vector4 uvTopLeftBottomRight,
	                                 uint8_t *rgbaColour,
	                                 Ogre::Vector2 parentDerivedTL,
	                                 Ogre::Vector2 parentDerivedBR,
	                                 Ogre::Vector2 invSize )
	{
		TODO_this_is_a_workaround_neg_y;
        #define CRYSTAL_ADD_VERTEX( _x, _y, _u, _v, clipDistanceTop, clipDistanceLeft, \
                                    clipDistanceRight, clipDistanceBottom ) \
            vertexBuffer->x = _x; \
            vertexBuffer->y = -_y; \
            vertexBuffer->u = static_cast<uint16_t>( _u * 65535.0f ); \
            vertexBuffer->v = static_cast<uint16_t>( _v * 65535.0f ); \
            vertexBuffer->rgbaColour[0] = rgbaColour[0]; \
            vertexBuffer->rgbaColour[1] = rgbaColour[1]; \
            vertexBuffer->rgbaColour[2] = rgbaColour[2]; \
            vertexBuffer->rgbaColour[3] = rgbaColour[3]; \
            vertexBuffer->clipDistance[Borders::Top]	= clipDistanceTop; \
            vertexBuffer->clipDistance[Borders::Left]	= clipDistanceLeft; \
            vertexBuffer->clipDistance[Borders::Right]	= clipDistanceRight; \
            vertexBuffer->clipDistance[Borders::Bottom]	= clipDistanceBottom; \
            ++vertexBuffer;

		CRYSTAL_ADD_VERTEX( topLeft.x, topLeft.y,
		                    uvTopLeftBottomRight.x, uvTopLeftBottomRight.y,
		                    (topLeft.y - parentDerivedTL.y) * invSize.y,
		                    (topLeft.x - parentDerivedTL.x) * invSize.x,
		                    (parentDerivedBR.x - topLeft.x) * invSize.x,
		                    (parentDerivedBR.y - topLeft.y) * invSize.y );

		CRYSTAL_ADD_VERTEX( topLeft.x, bottomRight.y,
		                    uvTopLeftBottomRight.x, uvTopLeftBottomRight.w,
		                    (bottomRight.y - parentDerivedTL.y) * invSize.y,
		                    (topLeft.x - parentDerivedTL.x) * invSize.x,
		                    (parentDerivedBR.x - topLeft.x) * invSize.x,
		                    (parentDerivedBR.y - bottomRight.y) * invSize.y );

		CRYSTAL_ADD_VERTEX( bottomRight.x, bottomRight.y,
		                    uvTopLeftBottomRight.z, uvTopLeftBottomRight.w,
		                    (bottomRight.y - parentDerivedTL.y) * invSize.y,
		                    (bottomRight.x - parentDerivedTL.x) * invSize.x,
		                    (parentDerivedBR.x - bottomRight.x) * invSize.x,
		                    (parentDerivedBR.y - bottomRight.y) * invSize.y );

		CRYSTAL_ADD_VERTEX( bottomRight.x, bottomRight.y,
		                    uvTopLeftBottomRight.z, uvTopLeftBottomRight.w,
		                    (bottomRight.y - parentDerivedTL.y) * invSize.y,
		                    (bottomRight.x - parentDerivedTL.x) * invSize.x,
		                    (parentDerivedBR.x - bottomRight.x) * invSize.x,
		                    (parentDerivedBR.y - bottomRight.y) * invSize.y );

		CRYSTAL_ADD_VERTEX( bottomRight.x, topLeft.y,
		                    uvTopLeftBottomRight.z, uvTopLeftBottomRight.y,
		                    (topLeft.y - parentDerivedTL.y) * invSize.y,
		                    (bottomRight.x - parentDerivedTL.x) * invSize.x,
		                    (parentDerivedBR.x - bottomRight.x) * invSize.x,
		                    (parentDerivedBR.y - topLeft.y) * invSize.y );

		CRYSTAL_ADD_VERTEX( topLeft.x, topLeft.y,
		                    uvTopLeftBottomRight.x, uvTopLeftBottomRight.y,
		                    (topLeft.y - parentDerivedTL.y) * invSize.y,
		                    (topLeft.x - parentDerivedTL.x) * invSize.x,
		                    (parentDerivedBR.x - topLeft.x) * invSize.x,
		                    (parentDerivedBR.y - topLeft.y) * invSize.y );

        #undef CRYSTAL_ADD_VERTEX
    }
	//-------------------------------------------------------------------------
	inline void Renderable::fillBuffersAndCommands( UiVertex * crystalgui_nonnull * crystalgui_nonnull
													_vertexBuffer,
													GlyphVertex * crystalgui_nonnull * crystalgui_nonnull
													_textVertBuffer,
													const Ogre::Vector2 &parentPos,
													const Ogre::Vector2 &parentScrollPos,
													const Ogre::Matrix3 &parentRot,
													const Ogre::Vector2 &currentScrollPos,
													bool forWindows )
	{
		UiVertex * RESTRICT_ALIAS vertexBuffer = *_vertexBuffer;

		m_culled = true;

		if( forWindows )
		{
			if( m_parent && !m_parent->intersectsChild( this, parentScrollPos ) )
				return;
		}
		else
		{
			if( !m_parent->intersectsChild( this, parentScrollPos ) )
				return;
		}

		m_culled = false;

		updateDerivedTransform( parentPos, parentRot );

		uint8_t rgbaColour[4];
		rgbaColour[0] = static_cast<uint8_t>( m_colour.r * 255.0f + 0.5f );
		rgbaColour[1] = static_cast<uint8_t>( m_colour.g * 255.0f + 0.5f );
		rgbaColour[2] = static_cast<uint8_t>( m_colour.b * 255.0f + 0.5f );
		rgbaColour[3] = static_cast<uint8_t>( m_colour.a * 255.0f + 0.5f );

		Ogre::Vector2 parentDerivedTL;
		Ogre::Vector2 parentDerivedBR;

		Ogre::Vector2 invCanvasSize2x = m_manager->getInvCanvasSize2x();

		if( forWindows )
		{
			parentDerivedTL = -1.0f;
			parentDerivedBR = 1.0f;
		}
		else
		{
			parentDerivedTL = m_parent->m_derivedTopLeft +
							  m_parent->m_clipBorderTL * invCanvasSize2x;
			parentDerivedBR = m_parent->m_derivedBottomRight -
							  m_parent->m_clipBorderBR * invCanvasSize2x;
		}

		Ogre::Vector2 invSize = 1.0f / (parentDerivedBR - parentDerivedTL);

		const Ogre::Vector2 outerTopLeft		= this->m_derivedTopLeft;
		const Ogre::Vector2 outerBottomRight	= this->m_derivedBottomRight;

		const StateInformation stateInfo = m_stateInformation[m_currentState];

		const Ogre::Vector2 &pixelSize2x = m_manager->getPixelSize2x();

		const Ogre::Vector2 borderTopLeft( stateInfo.borderSize[Borders::Left] * pixelSize2x.x,
										   stateInfo.borderSize[Borders::Top] * pixelSize2x.y );
		const Ogre::Vector2 borderBottomRight( stateInfo.borderSize[Borders::Right] * pixelSize2x.x,
											   stateInfo.borderSize[Borders::Bottom] * pixelSize2x.y );
		const Ogre::Vector2 innerTopLeft		= outerTopLeft + mul( this->m_derivedOrientation,
																	  borderTopLeft );
		const Ogre::Vector2 innerBottomRight	= outerBottomRight - mul( this->m_derivedOrientation,
																		  borderBottomRight );
		TODO_borderRepeatSize;
//		stateInfo.borderRepeatSize[Borders::Left] / (innerBottomRight.x - innerTopLeft.x);
//		stateInfo.borderRepeatSize[Borders::Right] / (innerBottomRight.x - innerTopLeft.x);
//		stateInfo.borderRepeatSize[Borders::Top] / (innerBottomRight.y - innerTopLeft.y);
//		stateInfo.borderRepeatSize[Borders::Bottom] / (innerBottomRight.y - innerTopLeft.y);

		//1st row
		addQuad( vertexBuffer,
				 outerTopLeft, innerTopLeft,
				 stateInfo.uvTopLeftBottomRight[0],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;
		addQuad( vertexBuffer,
				 Ogre::Vector2( innerTopLeft.x, outerTopLeft.y ),
				 Ogre::Vector2( innerBottomRight.x, innerTopLeft.y ),
				 stateInfo.uvTopLeftBottomRight[1],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;
		addQuad( vertexBuffer,
				 Ogre::Vector2( innerBottomRight.x, outerTopLeft.y ),
				 Ogre::Vector2( outerBottomRight.x, innerTopLeft.y ),
				 stateInfo.uvTopLeftBottomRight[2],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;
		//2nd row
		addQuad( vertexBuffer,
				 Ogre::Vector2( outerTopLeft.x, innerTopLeft.y ),
				 Ogre::Vector2( innerTopLeft.x, innerBottomRight.y ),
				 stateInfo.uvTopLeftBottomRight[3],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;
		addQuad( vertexBuffer,
				 Ogre::Vector2( innerTopLeft.x, innerTopLeft.y ),
				 Ogre::Vector2( innerBottomRight.x, innerBottomRight.y ),
				 stateInfo.uvTopLeftBottomRight[4],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;
		addQuad( vertexBuffer,
				 Ogre::Vector2( innerBottomRight.x, innerTopLeft.y ),
				 Ogre::Vector2( outerBottomRight.x, innerBottomRight.y ),
				 stateInfo.uvTopLeftBottomRight[5],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;
		//3rd row
		addQuad( vertexBuffer,
				 Ogre::Vector2( outerTopLeft.x, innerBottomRight.y ),
				 Ogre::Vector2( innerTopLeft.x, outerBottomRight.y ),
				 stateInfo.uvTopLeftBottomRight[6],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;
		addQuad( vertexBuffer,
				 Ogre::Vector2( innerTopLeft.x, innerBottomRight.y ),
				 Ogre::Vector2( innerBottomRight.x, outerBottomRight.y ),
				 stateInfo.uvTopLeftBottomRight[7],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;
		addQuad( vertexBuffer,
				 Ogre::Vector2( innerBottomRight.x, innerBottomRight.y ),
				 Ogre::Vector2( outerBottomRight.x, outerBottomRight.y ),
				 stateInfo.uvTopLeftBottomRight[8],
				 rgbaColour, parentDerivedTL, parentDerivedBR, invSize );
		vertexBuffer += 6u;

		*_vertexBuffer = vertexBuffer;

		const Ogre::Matrix3 finalRot = this->m_derivedOrientation;
		WidgetVec::const_iterator itor = m_children.begin();
		WidgetVec::const_iterator end  = m_children.end();

		const Ogre::Vector2 outerTopLeftWithClipping = outerTopLeft +
													   (m_clipBorderTL - currentScrollPos) *
													   invCanvasSize2x;

		while( itor != end )
		{
			(*itor)->fillBuffersAndCommands( _vertexBuffer, _textVertBuffer,
											 outerTopLeftWithClipping, currentScrollPos, finalRot );
			++itor;
		}
	}
}

CRYSTALGUI_ASSUME_NONNULL_END
