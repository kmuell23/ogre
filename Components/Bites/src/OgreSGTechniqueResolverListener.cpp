module Ogre.Components.Bites;

import :SGTechniqueResolverListener;

import Ogre.Components.RTShaderSystem;
import Ogre.Core;

import <cstddef>;
import <string>;
import <vector>;

namespace OgreBites {

SGTechniqueResolverListener::SGTechniqueResolverListener(Ogre::RTShader::ShaderGenerator *pShaderGenerator)
{
    mShaderGenerator = pShaderGenerator;
}

auto SGTechniqueResolverListener::handleSchemeNotFound(unsigned short schemeIndex, const Ogre::String &schemeName, Ogre::Material *originalMaterial, unsigned short lodIndex, const Ogre::Renderable *rend) -> Ogre::Technique * {
    if (!mShaderGenerator->hasRenderState(schemeName))
    {
        return nullptr;
    }
    // Case this is the default shader generator scheme.

    // Create shader generated technique for this material.
    bool techniqueCreated = mShaderGenerator->createShaderBasedTechnique(
                *originalMaterial,
                Ogre::MaterialManager::DEFAULT_SCHEME_NAME,
                schemeName);

    if (!techniqueCreated)
    {
        return nullptr;
    }
    // Case technique registration succeeded.

    // Force creating the shaders for the generated technique.
    mShaderGenerator->validateMaterial(schemeName, *originalMaterial);

    // Grab the generated technique.
    Ogre::Material::Techniques::const_iterator it;
    for(it = originalMaterial->getTechniques().begin(); it != originalMaterial->getTechniques().end(); ++it)
    {
        Ogre::Technique* curTech = *it;

        if (curTech->getSchemeName() == schemeName)
        {
            return curTech;
        }
    }

    return nullptr;
}

auto SGTechniqueResolverListener::afterIlluminationPassesCreated(Ogre::Technique *tech) -> bool
{
    if(mShaderGenerator->hasRenderState(tech->getSchemeName()))
    {
        Ogre::Material* mat = tech->getParent();
        mShaderGenerator->validateMaterialIlluminationPasses(tech->getSchemeName(),
                                                             mat->getName(), mat->getGroup());
        return true;
    }
    return false;
}

auto SGTechniqueResolverListener::beforeIlluminationPassesCleared(Ogre::Technique *tech) -> bool
{
    if(mShaderGenerator->hasRenderState(tech->getSchemeName()))
    {
        Ogre::Material* mat = tech->getParent();
        mShaderGenerator->invalidateMaterialIlluminationPasses(tech->getSchemeName(),
                                                               mat->getName(), mat->getGroup());
        return true;
    }
    return false;
}

}
