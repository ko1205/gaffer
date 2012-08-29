//////////////////////////////////////////////////////////////////////////
//  
//  Copyright (c) 2012, John Haddon. All rights reserved.
//  
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//  
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//  
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//  
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  
//////////////////////////////////////////////////////////////////////////

#include "boost/tokenizer.hpp"

#include "Gaffer/Context.h"

#include "GafferScene/BranchCreator.h"

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

IE_CORE_DEFINERUNTIMETYPED( BranchCreator );

size_t BranchCreator::g_firstPlugIndex = 0;

BranchCreator::BranchCreator( const std::string &name )
	:	SceneProcessor( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringPlug( "parent" ) );
	addChild( new StringPlug( "name" ) );
}

BranchCreator::~BranchCreator()
{
}

Gaffer::StringPlug *BranchCreator::parentPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

const Gaffer::StringPlug *BranchCreator::parentPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

Gaffer::StringPlug *BranchCreator::namePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::StringPlug *BranchCreator::namePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 1 );
}

void BranchCreator::affects( const ValuePlug *input, AffectedPlugsContainer &outputs ) const
{
	SceneProcessor::affects( input, outputs );
	
	if( input->parent<ScenePlug>() == inPlug() )
	{
		outputs.push_back( outPlug()->getChild<ValuePlug>( input->getName() ) );
	}
	else if( input == parentPlug() || input == namePlug() )
	{
		outputs.push_back( outPlug() );
	}
}

void BranchCreator::hash( const Gaffer::ValuePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{	
	if( output->parent<ScenePlug>() == outPlug() )
	{
		if( output == outPlug()->globalsPlug() )
		{
			h = inPlug()->globalsPlug()->hash();
		}
		else
		{	
			ScenePath path = context->get<ScenePath>( "scene:path" );
			ScenePath parentPath, branchPath;
			parentAndBranchPaths( path, parentPath, branchPath );
			
			if( output == outPlug()->boundPlug() )
			{
				if( branchPath.size() )
				{
					SceneProcessor::hash( output, context, h );
					hashBranchBound( parentPath, branchPath, context, h );
				}
				else if( parentPath.size() )
				{
					h = hashOfTransformedChildBounds( path, outPlug() );
				}
				else
				{
					h = inPlug()->boundPlug()->hash();
				}
			}
			else if( output == outPlug()->transformPlug() )
			{
				if( branchPath.size() )
				{
					SceneProcessor::hash( output, context, h );
					hashBranchTransform( parentPath, branchPath, context, h );				
				}
				else
				{
					h = inPlug()->transformPlug()->hash();
				}
			}
			else if( output == outPlug()->attributesPlug() )
			{
				if( branchPath.size() )
				{
					SceneProcessor::hash( output, context, h );
					hashBranchAttributes( parentPath, branchPath, context, h );
				}
				else
				{
					h = inPlug()->attributesPlug()->hash();
				}		
			}
			else if( output == outPlug()->objectPlug() )
			{
				if( branchPath.size() )
				{
					SceneProcessor::hash( output, context, h );
					hashBranchObject( parentPath, branchPath, context, h );
				}
				else
				{
					h = inPlug()->objectPlug()->hash();
				}
			}
			else if( output == outPlug()->childNamesPlug() )
			{
				if( branchPath.size() )
				{
					SceneProcessor::hash( output, context, h );
					hashBranchChildNames( parentPath, branchPath, context, h );
				}
				else if( path == parentPath )
				{
					SceneProcessor::hash( output, context, h );
					namePlug()->hash( h );
				}
				else
				{
					h = inPlug()->childNamesPlug()->hash();
				}
			}
		}
	}
	else
	{
		SceneProcessor::hash( output, context, h );
	}
}

Imath::Box3f BranchCreator::computeBound( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	ScenePath parentPath, branchPath;
	parentAndBranchPaths( path, parentPath, branchPath );
	if( branchPath.size() )
	{
		return computeBranchBound( parentPath, branchPath, context );
	}
	else if( parentPath.size() )
	{
		return unionOfTransformedChildBounds( path, outPlug() );
	}
	else
	{
		return inPlug()->boundPlug()->getValue();
	}
}

Imath::M44f BranchCreator::computeTransform( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	ScenePath parentPath, branchPath;
	parentAndBranchPaths( path, parentPath, branchPath );
	if( branchPath.size() )
	{
		return computeBranchTransform( parentPath, branchPath, context );
	}
	else
	{
		return inPlug()->transformPlug()->getValue();
	}
}

IECore::ConstCompoundObjectPtr BranchCreator::computeAttributes( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	ScenePath parentPath, branchPath;
	parentAndBranchPaths( path, parentPath, branchPath );
	if( branchPath.size() )
	{
		return computeBranchAttributes( parentPath, branchPath, context );
	}
	else
	{
		return inPlug()->attributesPlug()->getValue();
	}
}

IECore::ConstObjectPtr BranchCreator::computeObject( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	ScenePath parentPath, branchPath;
	parentAndBranchPaths( path, parentPath, branchPath );
	if( branchPath.size() )
	{
		return computeBranchObject( parentPath, branchPath, context );
	}
	else
	{
		return inPlug()->objectPlug()->getValue();
	}
}

IECore::ConstStringVectorDataPtr BranchCreator::computeChildNames( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	ScenePath parentPath, branchPath;
	parentAndBranchPaths( path, parentPath, branchPath );
	if( branchPath.size() )
	{
		return computeBranchChildNames( parentPath, branchPath, context );
	}
	else if( path == parentPath )
	{
		/// \todo Perhaps allow any existing children to coexist?
		/// If we do that then we know that the bound at the parent is just the union
		/// of the old bound and the bound at the root of the branch. we could then
		/// optimise the propagation of the bound back to the root by just unioning the
		/// appropriately transformed branch bound with the bound from the input scene.
		StringVectorDataPtr result = new StringVectorData;
		result->writable().push_back( namePlug()->getValue() );
		return result;		
	}
	else
	{
		return inPlug()->childNamesPlug()->getValue();
	}
}

IECore::ConstObjectVectorPtr BranchCreator::computeGlobals( const Gaffer::Context *context, const ScenePlug *parent ) const
{
	return inPlug()->globalsPlug()->getValue();
}

void BranchCreator::parentAndBranchPaths( const ScenePath &path, ScenePath &parentPath, ScenePath &branchPath ) const
{
	string parent = parentPlug()->getValue();
	string name = namePlug()->getValue();
	
	typedef boost::tokenizer<boost::char_separator<char> > Tokenizer;

	Tokenizer parentTokenizer( parent, boost::char_separator<char>( "/" ) );	
	Tokenizer pathTokenizer( path, boost::char_separator<char>( "/" ) );
	
	Tokenizer::iterator parentIterator, pathIterator;
	
	for(
		parentIterator = parentTokenizer.begin(), pathIterator = pathTokenizer.begin();	
		parentIterator != parentTokenizer.end() && pathIterator != pathTokenizer.end();
		parentIterator++, pathIterator++
	)
	{
		if( *parentIterator != *pathIterator )
		{
			return;
		}
	}
	
	if( pathIterator == pathTokenizer.end() )
	{
		// ancestor of parent, or parent itself
		parentPath = parent;
		return;
	}
	
	if( *pathIterator++ != name )
	{
		// another child of parent, one we don't need to worry about
		return;
	}
	
	// somewhere on the new branch
	parentPath = parent;
	branchPath = "/";
	while( pathIterator != pathTokenizer.end() )
	{
		if( branchPath.size() > 1 )
		{
			branchPath += "/";
		}
		branchPath += *pathIterator++;
	}
}
