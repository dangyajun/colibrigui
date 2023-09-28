
#include "ColibriGui/ColibriGraphChart.h"

#include "ColibriGui/ColibriLabel.h"
#include "ColibriGui/ColibriManager.h"
#include "ColibriGui/Layouts/ColibriLayoutLine.h"
#include "ColibriGui/Layouts/ColibriLayoutTableSameSize.h"

#include "OgreHlms.h"
#include "OgreHlmsManager.h"
#include "OgreHlmsUnlitDatablock.h"
#include "OgreRenderSystem.h"
#include "OgreStagingTexture.h"
#include "OgreTextureGpu.h"
#include "OgreTextureGpuManager.h"

using namespace Colibri;

GraphChart::GraphChart( ColibriManager *manager ) :
	CustomShape( manager ),
	m_textureData( 0 ),
	m_labelsDirty( true ),
	m_autoMin( false ),
	m_autoMax( false ),
	m_minSample( 0.0f ),
	m_maxSample( 1.0f )
{
	setCustomParameter( 6374, Ogre::Vector4( 1.0f ) );
}
//-------------------------------------------------------------------------
void GraphChart::_initialize()
{
	CustomShape::_initialize();
	_setSkinPack( m_manager->getDefaultSkin( SkinWidgetTypes::GraphChart ) );

	// We must clone our datablocks because they will have a unique texture.
	Ogre::HlmsManager *hlmsManager = m_manager->getOgreHlmsManager();
	for( size_t i = 0; i < States::NumStates; ++i )
	{
		Ogre::HlmsDatablock *refDatablock =
			hlmsManager->getDatablockNoDefault( m_stateInformation[i].materialName );

		char tmpBuffer[64];
		Ogre::LwString idStr( Ogre::LwString::FromEmptyPointer( tmpBuffer, sizeof( tmpBuffer ) ) );
		idStr.a( "_", getId() );
		const Ogre::String newName = *refDatablock->getNameStr() + idStr.c_str();

		Ogre::HlmsDatablock *datablock = hlmsManager->getDatablockNoDefault( newName );
		if( !datablock )
			datablock = refDatablock->clone( newName );

		m_stateInformation[i].materialName = datablock->getName();

		if( i == m_currentState )
			setDatablock( datablock );
	}

	setNumTriangles( 2u );
	setQuad( 0u, Ogre::Vector2( -1.0f ), Ogre::Vector2( 2.0f ), Ogre::ColourValue::White,
			 Ogre::Vector2( 0.0f, 1.0f ), Ogre::Vector2( 1.0f, -1.0f ) );
}
//-------------------------------------------------------------------------
void GraphChart::_destroy()
{
	this->_setNullDatablock();

	// Destroy the datablock clones we created.
	Ogre::HlmsManager *hlmsManager = m_manager->getOgreHlmsManager();
	std::set<Ogre::IdString> seenDatablocks;
	for( size_t i = 0; i < States::NumStates; ++i )
	{
		if( seenDatablocks.count( m_stateInformation[i].materialName ) != 0u )
		{
			Ogre::HlmsDatablock *datablock =
				hlmsManager->getDatablockNoDefault( m_stateInformation[i].materialName );
			datablock->getCreator()->destroyDatablock( m_stateInformation[i].materialName );
			seenDatablocks.insert( m_stateInformation[i].materialName );
		}
	}

	if( m_textureData )
	{
		Ogre::TextureGpuManager *textureManager = m_textureData->getTextureManager();
		textureManager->destroyTexture( m_textureData );
		m_textureData = 0;
	}

	// No need to destroy m_labels. They are our children and will be destroyed
	// automatically with us.
	CustomShape::_destroy();
}
//-------------------------------------------------------------------------
void GraphChart::setMaxValues( const uint32_t numColumns, const uint32_t maxEntriesPerColumn )
{
	if( numColumns > m_columns.size() )
	{
		// Destroy excess labels.
		std::vector<Column>::iterator itor = m_columns.begin();
		std::vector<Column>::iterator endt = m_columns.end();

		while( itor != endt )
		{
			m_manager->destroyWidget( itor->label );
			m_manager->destroyWidget( itor->rectangle );
			++itor;
		}
	}

	m_columns.resize( numColumns );
	m_allValues.clear();
	m_allValues.resize( numColumns * maxEntriesPerColumn, 0.0f );

	Ogre::HlmsManager *hlmsManager = m_manager->getOgreHlmsManager();

	for( size_t y = 0u; y < numColumns; ++y )
	{
		m_columns[y].values = &m_allValues[y * maxEntriesPerColumn];
		m_columns[y].label = m_manager->createWidget<Label>( this );
		m_columns[y].rectangle = m_manager->createWidget<CustomShape>( this );

		m_columns[y].rectangle->setDatablock(
			hlmsManager->getHlms( Ogre::HLMS_UNLIT )->getDefaultDatablock() );
		m_columns[y].rectangle->setNumTriangles( 2u );
		m_columns[y].rectangle->setQuad( 0u, Ogre::Vector2( -1.0f ), Ogre::Vector2( 2.0f ),
										 Ogre::ColourValue::White, Ogre::Vector2::ZERO,
										 Ogre::Vector2::UNIT_SCALE );

		m_columns[y].label->m_gridLocation = GridLocations::CenterLeft;
		m_columns[y].rectangle->m_gridLocation = GridLocations::CenterLeft;
		m_columns[y].label->m_margin = 10.0f;
	}

	m_labelsDirty = true;

	if( m_textureData && m_textureData->getHeight() == numColumns &&
		m_textureData->getWidth() == maxEntriesPerColumn )
	{
		// We're done.
		return;
	}

	if( m_textureData )
	{
		m_textureData->scheduleTransitionTo( Ogre::GpuResidency::OnStorage );
	}
	else
	{
		char tmpBuffer[64];
		Ogre::LwString texName( Ogre::LwString::FromEmptyPointer( tmpBuffer, sizeof( tmpBuffer ) ) );
		texName.a( "GraphChart #", getId() );

		Ogre::TextureGpuManager *textureManager =
			m_manager->getOgreHlmsManager()->getRenderSystem()->getTextureGpuManager();

		m_textureData = textureManager->createTexture(
			texName.c_str(), Ogre::GpuPageOutStrategy::Discard, Ogre::TextureFlags::ManualTexture,
			Ogre::TextureTypes::Type2D );
	}

	m_textureData->setResolution( maxEntriesPerColumn, numColumns );
	m_textureData->setPixelFormat( Ogre::PFG_R16_UNORM );
	m_textureData->scheduleTransitionTo( Ogre::GpuResidency::Resident );

	// Set the texture to our datablock.
	for( size_t i = 0; i < States::NumStates; ++i )
	{
		Ogre::HlmsDatablock *datablock =
			hlmsManager->getDatablockNoDefault( m_stateInformation[i].materialName );

		COLIBRI_ASSERT_HIGH( dynamic_cast<Ogre::HlmsUnlitDatablock *>( datablock ) );
		Ogre::HlmsUnlitDatablock *unlitDatablock = static_cast<Ogre::HlmsUnlitDatablock *>( datablock );
		unlitDatablock->setTexture( 0u, m_textureData );
	}

	syncChart();
}
//-------------------------------------------------------------------------
uint32_t GraphChart::getEntriesPerColumn() const
{
	if( !m_textureData )
		return 0u;
	return m_textureData->getWidth();
}
//-------------------------------------------------------------------------
void GraphChart::syncChart()
{
	Ogre::TextureGpuManager *textureManager = m_textureData->getTextureManager();
	Ogre::StagingTexture *stagingTexture = textureManager->getStagingTexture(
		m_textureData->getWidth(), m_textureData->getHeight(), 1u, 1u, Ogre::PFG_R16_UNORM, 100u );

	stagingTexture->startMapRegion();
	Ogre::TextureBox textureBox = stagingTexture->mapRegion(
		m_textureData->getWidth(), m_textureData->getHeight(), 1u, 1u, Ogre::PFG_R16_UNORM );

	COLIBRI_ASSERT_MEDIUM( m_textureData->getWidth() == getEntriesPerColumn() );
	COLIBRI_ASSERT_MEDIUM( m_textureData->getHeight() == m_columns.size() );

	float minSample = m_minSample;
	float maxSample = m_maxSample;

	{
		const bool autoMin = m_autoMin;
		const bool autoMax = m_autoMax;

		if( autoMin || autoMax )
		{
			for( size_t y = 0u; y < textureBox.height; ++y )
			{
				for( size_t x = 0u; x < textureBox.width; ++x )
				{
					if( autoMin )
						minSample = std::min( m_columns[y].values[x], minSample );
					if( autoMax )
						maxSample = std::max( m_columns[y].values[x], maxSample );
				}
			}
		}
	}

	const float sampleInterval =
		( maxSample - minSample ) < 1e-6f ? 1.0f : ( 1.0f / ( maxSample - minSample ) );

	for( size_t y = 0u; y < textureBox.height; ++y )
	{
		uint16_t *RESTRICT_ALIAS dstData =
			reinterpret_cast<uint16_t * RESTRICT_ALIAS>( textureBox.at( 0u, y, 0u ) );
		for( size_t x = 0u; x < textureBox.width; ++x )
		{
			float fValue =
				Ogre::Math::saturate( ( m_columns[y].values[x] - minSample ) * sampleInterval );
			dstData[x] = static_cast<uint16_t>( fValue * 65535.0f );
		}
	}

	stagingTexture->stopMapRegion();
	stagingTexture->upload( textureBox, m_textureData, 0u );

	// Workaround OgreNext bug where calling syncChart() multiple times in a row causes Vulkan
	// race conditions because it doesn't place barriers to protect each copy as it assumes
	// multiple write copies to the same textures are to be done to non-overlapping regions.
	m_manager->getOgreHlmsManager()->getRenderSystem()->endCopyEncoder();

	if( m_labelsDirty )
	{
		{
			float labelHeight = std::numeric_limits<float>::max();
			for( const Column &column : m_columns )
				labelHeight = std::min( labelHeight, column.label->getSize().y * 0.33f );
			for( const Column &column : m_columns )
			{
				column.rectangle->setSizeAndCellMinSize(
					Ogre::Vector2( labelHeight * 1.33f, labelHeight ) );
			}
		}

		const size_t numColumns = m_columns.size();

		LayoutTableSameSize rootLayout( m_manager );
		std::vector<LayoutLine> columnLines;
		columnLines.resize( m_columns.size(), m_manager );

		for( size_t y = 0u; y < numColumns; ++y )
		{
			columnLines[y].m_vertical = false;
			columnLines[y].m_expand[0] = true;
			columnLines[y].m_expand[1] = true;
			columnLines[y].m_proportion[0] = 1u;
			columnLines[y].m_proportion[1] = 1u;
			columnLines[y].addCell( m_columns[y].rectangle );
			columnLines[y].addCell( m_columns[y].label );
			rootLayout.addCell( &columnLines[y] );
		}

		// rootLayout.setCellSize()
		rootLayout.layout();
	}
}
//-------------------------------------------------------------------------
void GraphChart::setTransformDirty( uint32_t dirtyReason )
{
	// Only update if our size is directly being changed, not our parent's
	if( ( dirtyReason & ( TransformDirtyParentCaller | TransformDirtyScale ) ) == TransformDirtyScale )
		m_labelsDirty = true;

	Widget::setTransformDirty( dirtyReason );
}
