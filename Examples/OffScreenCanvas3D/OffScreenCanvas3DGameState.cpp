
#include "OffScreenCanvas3DGameState.h"

#include "CameraController.h"
#include "ExamplesCommon.h"

#include "OgreCamera.h"
#include "OgreFrameStats.h"
#include "OgreHlmsManager.h"
#include "OgreHlmsPbs.h"
#include "OgreHlmsPbsDatablock.h"
#include "OgreHlmsSamplerblock.h"
#include "OgreHlmsUnlitDatablock.h"
#include "OgreItem.h"
#include "OgreLogManager.h"
#include "OgreMesh2.h"
#include "OgreMeshManager.h"
#include "OgreMeshManager2.h"
#include "OgreRectangle2D2.h"
#include "OgreRoot.h"
#include "OgreSceneManager.h"
#include "OgreTextureGpuManager.h"
#include "OgreWindow.h"

#include "SdlInputHandler.h"

#include "ColibriGui/ColibriButton.h"
#include "ColibriGui/ColibriCheckbox.h"
#include "ColibriGui/ColibriEditbox.h"
#include "ColibriGui/ColibriGraphChart.h"
#include "ColibriGui/ColibriLabel.h"
#include "ColibriGui/ColibriManager.h"
#include "ColibriGui/ColibriOffScreenCanvas.h"
#include "ColibriGui/ColibriProgressbar.h"
#include "ColibriGui/ColibriRadarChart.h"
#include "ColibriGui/ColibriSlider.h"
#include "ColibriGui/ColibriSpinner.h"
#include "ColibriGui/ColibriToggleButton.h"
#include "ColibriGui/ColibriWindow.h"

#include "ColibriGui/Layouts/ColibriLayoutLine.h"
#include "ColibriGui/Layouts/ColibriLayoutMultiline.h"
#include "ColibriGui/Layouts/ColibriLayoutTableSameSize.h"

using namespace Demo;

static Colibri::Window *fullWindow = 0;
static Colibri::Button *button0 = 0;

class DemoWidgetListener final : public Colibri::WidgetActionListener
{
public:
	~DemoWidgetListener() {}
	void notifyWidgetAction( Colibri::Widget *widget, Colibri::Action::Action action ) override
	{
		if( action == Colibri::Action::Action::PrimaryActionPerform )
		{
		}
	}
};
static DemoWidgetListener *demoActionListener;

OffScreenCanvas3DGameState::OffScreenCanvas3DGameState( const Ogre::String &helpDescription ) :
	TutorialGameState( helpDescription ),
	mText3D{},
	mNodes{},
	mOffscreenCanvas( 0 ),
	mLabelOffscreen( 0 )
{
}
//-----------------------------------------------------------------------------------
Colibri::ColibriManager *OffScreenCanvas3DGameState::getColibriManager()
{
	COLIBRI_ASSERT_HIGH( dynamic_cast<ColibriGuiGraphicsSystem *>( mGraphicsSystem ) );
	return static_cast<ColibriGuiGraphicsSystem *>( mGraphicsSystem )->getColibriManager();
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::drawTextIn3D( Ogre::Rectangle2D *rectangle, const char *text,
											   float scaleInWorldUnits )
{
	Ogre::HlmsUnlitDatablock *unlitDatablock = 0;

	if( !rectangle->getDatablock() ||
		rectangle->getDatablock()->getCreator()->getType() != Ogre::HLMS_UNLIT )
	{
		// Create a datablock (aka material) to hold the texture we're about to render to.
		// We use Unlit because it's often what the user would want. But you could use PBS
		// if you want the text to be affected by lighting (or assign the texture as emissive).
		Ogre::Hlms *hlms = mGraphicsSystem->getRoot()->getHlmsManager()->getHlms( Ogre::HLMS_UNLIT );

		char tmpBuffer[64];
		Ogre::LwString matName( Ogre::LwString::FromEmptyPointer( tmpBuffer, sizeof( tmpBuffer ) ) );
		matName.a( "3D Text Material #", Ogre::Id::generateNewId<OffScreenCanvas3DGameState>() );

		Ogre::HlmsMacroblock macroblock;
		Ogre::HlmsBlendblock blendblock;

		macroblock.mCullMode = Ogre::CULL_NONE;
		blendblock.setBlendType( Ogre::SBT_TRANSPARENT_ALPHA );

		Ogre::HlmsDatablock *datablock = hlms->createDatablock(
			matName.c_str(), matName.c_str(), macroblock, blendblock, Ogre::HlmsParamVec() );

		COLIBRI_ASSERT_HIGH( dynamic_cast<Ogre::HlmsUnlitDatablock *>( datablock ) );
		unlitDatablock = static_cast<Ogre::HlmsUnlitDatablock *>( datablock );

		// Initialize geometry or else setDatablock() will fail.
		rectangle->setDatablock( unlitDatablock );
	}
	else
	{
		// We've already created this datablock. NOTE: We assume that if it's unlit then it was created
		// by us. This may not actually be the case. OgreNext defaults datablocks to PBS so we *assume*
		// if we see nothing or a PBS datablock, then it's using the default datablock.
		COLIBRI_ASSERT_HIGH( dynamic_cast<Ogre::HlmsUnlitDatablock *>( rectangle->getDatablock() ) );
		unlitDatablock = static_cast<Ogre::HlmsUnlitDatablock *>( rectangle->getDatablock() );
	}

	{
		// Destroy the previous texture (if we already called drawTextIn3D on this rectangle).
		Ogre::TextureGpu *oldTexture = unlitDatablock->getTexture( 0u );
		if( oldTexture )
		{
			unlitDatablock->setTexture( 0u, (Ogre::TextureGpu *)nullptr );
			oldTexture->getTextureManager()->destroyTexture( oldTexture );
		}
	}

	// We must set some size here otherwise pixel-perfect snapping of Label calculations
	// will be messed up, causing sizeToFit() to fail.
	mOffscreenCanvas->getSecondaryManager()->setCanvasSize( Ogre::Vector2( 1024.0f ),
															Ogre::Vector2( 1024.0f ) );

	// Set the text and calculate its size.
	mLabelOffscreen->setHidden( false );
	mLabelOffscreen->setText( text );
	mLabelOffscreen->sizeToFit();

	{
		// Text's size calculated. Now make the canvas & texture resolution to tightly match the label.
		const Ogre::Vector2 texResolution( std::ceil( mLabelOffscreen->getSize().x ),
										   std::ceil( mLabelOffscreen->getSize().y ) );

		mOffscreenCanvas->getSecondaryManager()->setCanvasSize( mLabelOffscreen->getSize(),
																texResolution );
		// Set the window to match to avoid getting clipped.
		mLabelOffscreen->getParent()->setSize( mLabelOffscreen->getSize() );

		mOffscreenCanvas->createTexture( uint32_t( texResolution.x ), uint32_t( texResolution.y ) );
		COLIBRI_ASSERT_HIGH( dynamic_cast<ColibriGuiGraphicsSystem *>( mGraphicsSystem ) );
		mOffscreenCanvas->createWorkspace(
			static_cast<ColibriGuiGraphicsSystem *>( mGraphicsSystem )->getColibriCompoProvider(),
			mGraphicsSystem->getCamera() );

		const Ogre::Vector2 rectSize( scaleInWorldUnits * texResolution /
									  std::max( texResolution.x, texResolution.y ) );

		rectangle->setGeometry( -rectSize * 0.5f, rectSize );
		rectangle->update();
	}

	// Render the text.
	mOffscreenCanvas->updateCanvas( 0.0f );

	// Asign the rendered texture to the rectangle's material.
	Ogre::TextureGpu *outputTexture = mOffscreenCanvas->disownCanvasTexture( false );
	unlitDatablock->setTexture( 0u, outputTexture );

	// We hide this label in case we'd want to render other unrelated things.
	mLabelOffscreen->setHidden( true );
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::createScene01( void )
{
	mCameraController = new CameraController( mGraphicsSystem, false );

	Ogre::Window *window = mGraphicsSystem->getRenderWindow();
	Colibri::ColibriManager *colibriManager = getColibriManager();

	const float aspectRatioColibri =
		static_cast<float>( window->getHeight() ) / static_cast<float>( window->getWidth() );
	colibriManager->setCanvasSize( Ogre::Vector2( 1920.0f, 1920.0f * aspectRatioColibri ),
								   Ogre::Vector2( window->getWidth(), window->getHeight() ) );

	// colibriManager = new Colibri::ColibriManager();
	colibriManager->setOgre( mGraphicsSystem->getRoot(),
							 mGraphicsSystem->getRoot()->getRenderSystem()->getVaoManager(),
							 mGraphicsSystem->getSceneManager() );
	colibriManager->loadSkins( ( mGraphicsSystem->getResourcePath() +
								 "Materials/ColibriGui/Skins/DarkGloss/Skins.colibri.json" )
								   .c_str() );

	// colibriManager->setTouchOnlyMode( true );

	fullWindow = colibriManager->createWindow( 0 );
	fullWindow->setSize( colibriManager->getCanvasSize() );

	// Ogre::Hlms *hlms = mGraphicsSystem->getRoot()->getHlmsManager()->getHlms( Ogre::HLMS_UNLIT );
	// mainWindow->setDatablock( hlms->getDefaultDatablock() );

	// When m_breadthFirst is set to true, it can cause significant performance
	// increases for UI-heavy applications. But be sure you understand it i.e.
	// it may not render correctly if your widgets have children and they overlap.
	fullWindow->m_breadthFirst = true;

	fullWindow->setSize( colibriManager->getCanvasSize() );
	fullWindow->setSkin( "EmptyBg" );
	fullWindow->setVisualsEnabled( false );
	{
		Colibri::Label *emojiText = colibriManager->createWidget<Colibri::Label>( fullWindow );
		emojiText->setText(
			"By defining a BMP font you can easily use the private area\n"
			"to draw emoji-like icons. Big Coffe cup: \uE000 Pizza: \uE001\n"
			"This is a new line after emojis." );
		emojiText->sizeToFit();
		emojiText->setTopLeft( fullWindow->getSize() - emojiText->getSize() );
	}

	// Overlapping windows
	demoActionListener = new DemoWidgetListener();

	mOffscreenCanvas = new Colibri::OffScreenCanvas( colibriManager );

	Colibri::ColibriManager *secondaryManager = mOffscreenCanvas->getSecondaryManager();
	Colibri::Window *offscreenRoot = secondaryManager->createWindow( 0 );
	offscreenRoot->setSkin( "EmptyBg" );
	offscreenRoot->setVisualsEnabled( false );
	offscreenRoot->m_breadthFirst = true;

	mLabelOffscreen = secondaryManager->createWidget<Colibri::Label>( offscreenRoot );
	// Bigger font size = higher quality (doesn't change the in-world size) & bigger texture resolution.
	mLabelOffscreen->setDefaultFontSize( 32.0f );
	mLabelOffscreen->setShadowOutline( true, Ogre::ColourValue::Black, Ogre::Vector2( 2.0f ) );

	Ogre::SceneManager *sceneManager = mGraphicsSystem->getSceneManager();
	for( size_t i = 0u; i < kNum3DTexts; ++i )
	{
		mText3D[i] = OGRE_NEW Ogre::Rectangle2D(
			Ogre::Id::generateNewId<Ogre::MovableObject>(),
			&sceneManager->_getEntityMemoryManager( Ogre::SCENE_DYNAMIC ), sceneManager );
		mText3D[i]->initialize( Ogre::BT_DEFAULT, Ogre::Rectangle2D::GeometryFlagQuad );
		mText3D[i]->setUseIdentityView( false );
		mText3D[i]->setUseIdentityProjection( false );
		mNodes[i] = sceneManager->getRootSceneNode()->createChildSceneNode();
		mNodes[i]->attachObject( mText3D[i] );
		// drawTextIn3D( mText3D[i], "THIS is a teST" );
	}

	TutorialGameState::createScene01();
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::destroyScene()
{
	if( mLabelOffscreen )
	{
		// Traverse & remove in LIFO order for faster shutdown.
		for( size_t i = kNum3DTexts; i--; )
		{
			mNodes[i]->getParentSceneNode()->removeAndDestroyChild( mNodes[i] );
			OGRE_DELETE mText3D[i];
			mText3D[i] = 0;
		}

		// Destroy the OffScreen's root window (which also destroys mLabelOffscreen).
		mOffscreenCanvas->getSecondaryManager()->destroyWidget( mLabelOffscreen->getParent() );
		mLabelOffscreen = 0;

		delete mOffscreenCanvas;  // We MUST delete the OffScreenCanvas before the main ColibriManager.
		mOffscreenCanvas = 0;
	}

	Colibri::ColibriManager *colibriManager = getColibriManager();
	colibriManager->destroyWindow( fullWindow );
	delete demoActionListener;
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::update( float timeSinceLast )
{
	Colibri::ColibriManager *colibriManager = getColibriManager();

	static bool tried = false;
	if( !tried )
	{
		SdlInputHandler *inputHandler = mGraphicsSystem->getInputHandler();
		inputHandler->setGrabMousePointer( false );
		inputHandler->setMouseVisible( true );
		inputHandler->setMouseRelative( false );
		tried = true;
	}

	colibriManager->update( timeSinceLast );

	const bool isTextInputActive = SDL_IsTextInputActive();

	if( colibriManager->focusedWantsTextInput() && !isTextInputActive )
		SDL_StartTextInput();
	else if( !colibriManager->focusedWantsTextInput() && isTextInputActive )
		SDL_StopTextInput();

	if( isTextInputActive )
	{
		static SDL_Rect oldRect = { 0, 0, 0, 0 };
		// We tried to update this only inside OffScreenCanvas3DGameState::keyPressed
		// but it didn't work well in Linux with fcitx
		Ogre::Vector2 imeOffset = colibriManager->getImeLocation();
		SDL_Rect rect;
		rect.x = imeOffset.x;
		rect.y = imeOffset.y;
		rect.w = 0;
		rect.h = 0;
		if( oldRect.x != rect.x || oldRect.y != rect.y )
			SDL_SetTextInputRect( &rect );
	}

	/*static float angle = 0;
	Ogre::Matrix3 rotMat;
	rotMat.FromEulerAnglesXYZ( Ogre::Degree( 0 ), Ogre::Radian( 0 ), Ogre::Radian( angle ) );
	button->setOrientation( rotMat );
	angle += timeSinceLast;*/

	// mOffscreenCanvas->updateCanvas( 0.0f );
	drawTextIn3D( mText3D[0], "THIS is a teST", 4.0f );

	mNodes[0]->roll( Ogre::Radian( -timeSinceLast * 0.35f ) );
	mNodes[0]->yaw( Ogre::Radian( -timeSinceLast ) );

	TutorialGameState::update( timeSinceLast );
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::generateDebugText( float timeSinceLast, Ogre::String &outText )
{
	TutorialGameState::generateDebugText( timeSinceLast, outText );
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::mouseMoved( const SDL_Event &arg )
{
	Colibri::ColibriManager *colibriManager = getColibriManager();

	float width = static_cast<float>( mGraphicsSystem->getRenderWindow()->getWidth() );
	float height = static_cast<float>( mGraphicsSystem->getRenderWindow()->getHeight() );

	if( arg.type == SDL_MOUSEMOTION )
	{
		Ogre::Vector2 mousePos( arg.motion.x / width, arg.motion.y / height );
		colibriManager->setMouseCursorMoved( mousePos * colibriManager->getCanvasSize() );
	}
	else if( arg.type == SDL_MOUSEWHEEL )
	{
		Ogre::Vector2 mouseScroll( arg.wheel.x, -arg.wheel.y );
		colibriManager->setScroll( mouseScroll * 50.0f * colibriManager->getCanvasSize() *
								   colibriManager->getPixelSize() );
	}

	TutorialGameState::mouseMoved( arg );
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::mousePressed( const SDL_MouseButtonEvent &arg, Ogre::uint8 id )
{
	Colibri::ColibriManager *colibriManager = getColibriManager();

	float width = static_cast<float>( mGraphicsSystem->getRenderWindow()->getWidth() );
	float height = static_cast<float>( mGraphicsSystem->getRenderWindow()->getHeight() );

	Ogre::Vector2 mousePos( arg.x / width, arg.y / height );
	colibriManager->setMouseCursorMoved( mousePos * colibriManager->getCanvasSize() );
	colibriManager->setMouseCursorPressed( true, false );
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::mouseReleased( const SDL_MouseButtonEvent &arg, Ogre::uint8 id )
{
	Colibri::ColibriManager *colibriManager = getColibriManager();
	colibriManager->setMouseCursorReleased();
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::textEditing( const SDL_TextEditingEvent &arg )
{
	Colibri::ColibriManager *colibriManager = getColibriManager();
	colibriManager->setTextEdit( arg.text, arg.start, arg.length );
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::textInput( const SDL_TextInputEvent &arg )
{
	Colibri::ColibriManager *colibriManager = getColibriManager();
	colibriManager->setTextInput( arg.text, false );
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::keyPressed( const SDL_KeyboardEvent &arg )
{
	Colibri::ColibriManager *colibriManager = getColibriManager();

	const bool isTextInputActive = SDL_IsTextInputActive();
	const bool isTextMultiline = colibriManager->isTextMultiline();

	if( ( arg.keysym.sym == SDLK_w && !isTextInputActive ) || arg.keysym.sym == SDLK_UP )
		colibriManager->setKeyDirectionPressed( Colibri::Borders::Top );
	else if( ( arg.keysym.sym == SDLK_s && !isTextInputActive ) || arg.keysym.sym == SDLK_DOWN )
		colibriManager->setKeyDirectionPressed( Colibri::Borders::Bottom );
	else if( ( arg.keysym.sym == SDLK_a && !isTextInputActive ) || arg.keysym.sym == SDLK_LEFT )
		colibriManager->setKeyDirectionPressed( Colibri::Borders::Left );
	else if( ( arg.keysym.sym == SDLK_d && !isTextInputActive ) || arg.keysym.sym == SDLK_RIGHT )
		colibriManager->setKeyDirectionPressed( Colibri::Borders::Right );
	else if( ( ( arg.keysym.sym == SDLK_RETURN || arg.keysym.sym == SDLK_KP_ENTER ) &&
			   !isTextMultiline ) ||
			 ( arg.keysym.sym == SDLK_SPACE && !isTextInputActive ) )
	{
		colibriManager->setKeyboardPrimaryPressed();
	}
	else if( isTextInputActive )
	{
		if( ( arg.keysym.sym == SDLK_RETURN || arg.keysym.sym == SDLK_KP_ENTER ) && isTextMultiline )
			colibriManager->setTextSpecialKeyPressed( SDLK_RETURN, arg.keysym.mod );
		else
		{
			colibriManager->setTextSpecialKeyPressed( arg.keysym.sym & ~SDLK_SCANCODE_MASK,
													  arg.keysym.mod );
		}
	}
}
//-----------------------------------------------------------------------------------
void OffScreenCanvas3DGameState::keyReleased( const SDL_KeyboardEvent &arg )
{
	if( ( arg.keysym.mod & ~( KMOD_NUM | KMOD_CAPS ) ) != 0 )
	{
		TutorialGameState::keyReleased( arg );
		return;
	}

	Colibri::ColibriManager *colibriManager = getColibriManager();

	const bool isTextInputActive = SDL_IsTextInputActive();
	const bool isTextMultiline = colibriManager->isTextMultiline();

	if( ( arg.keysym.sym == SDLK_w && !isTextInputActive ) || arg.keysym.sym == SDLK_UP )
		colibriManager->setKeyDirectionReleased( Colibri::Borders::Top );
	else if( ( arg.keysym.sym == SDLK_s && !isTextInputActive ) || arg.keysym.sym == SDLK_DOWN )
		colibriManager->setKeyDirectionReleased( Colibri::Borders::Bottom );
	else if( ( arg.keysym.sym == SDLK_a && !isTextInputActive ) || arg.keysym.sym == SDLK_LEFT )
		colibriManager->setKeyDirectionReleased( Colibri::Borders::Left );
	else if( ( arg.keysym.sym == SDLK_d && !isTextInputActive ) || arg.keysym.sym == SDLK_RIGHT )
		colibriManager->setKeyDirectionReleased( Colibri::Borders::Right );
	else if( ( ( arg.keysym.sym == SDLK_RETURN || arg.keysym.sym == SDLK_KP_ENTER ) &&
			   !isTextMultiline ) ||
			 ( arg.keysym.sym == SDLK_SPACE && !isTextInputActive ) )
	{
		colibriManager->setKeyboardPrimaryReleased();
	}
	else if( isTextInputActive )
	{
		if( ( arg.keysym.sym == SDLK_RETURN || arg.keysym.sym == SDLK_KP_ENTER ) && isTextMultiline )
			colibriManager->setTextSpecialKeyReleased( SDLK_RETURN, arg.keysym.mod );
		else
		{
			colibriManager->setTextSpecialKeyReleased( arg.keysym.sym & ~SDLK_SCANCODE_MASK,
													   arg.keysym.mod );
		}
	}

	TutorialGameState::keyReleased( arg );
}
