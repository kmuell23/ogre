/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
module;

#include <cassert>
#include <cmath>

module Ogre.Core;

import :Common;
import :ParticleEmitter;
import :ParticleEmitterCommands;
import :ParticleEmitterFactory;

import <vector>;

namespace Ogre
{
    // Command object for setting / getting parameters
    static EmitterCommands::CmdAngle msAngleCmd;
    static EmitterCommands::CmdColour msColourCmd;
    static EmitterCommands::CmdColourRangeStart msColourRangeStartCmd;
    static EmitterCommands::CmdColourRangeEnd msColourRangeEndCmd;
    static EmitterCommands::CmdDirection msDirectionCmd;
    static EmitterCommands::CmdUp msUpCmd;
    static EmitterCommands::CmdDirPositionRef msDirPositionRefCmd;
    static EmitterCommands::CmdEmissionRate msEmissionRateCmd;
    static EmitterCommands::CmdMaxTTL msMaxTTLCmd;
    static EmitterCommands::CmdMaxVelocity msMaxVelocityCmd;
    static EmitterCommands::CmdMinTTL msMinTTLCmd;
    static EmitterCommands::CmdMinVelocity msMinVelocityCmd;
    static EmitterCommands::CmdPosition msPositionCmd;
    static EmitterCommands::CmdTTL msTTLCmd;
    static EmitterCommands::CmdVelocity msVelocityCmd;
    static EmitterCommands::CmdDuration msDurationCmd;
    static EmitterCommands::CmdMinDuration msMinDurationCmd;
    static EmitterCommands::CmdMaxDuration msMaxDurationCmd;
    static EmitterCommands::CmdRepeatDelay msRepeatDelayCmd;
    static EmitterCommands::CmdMinRepeatDelay msMinRepeatDelayCmd;
    static EmitterCommands::CmdMaxRepeatDelay msMaxRepeatDelayCmd;
    static EmitterCommands::CmdName msNameCmd;
    static EmitterCommands::CmdEmittedEmitter msEmittedEmitterCmd;


    //-----------------------------------------------------------------------
    ParticleEmitter::ParticleEmitter(ParticleSystem* psys)
      : mParent(psys),
        
        mDirPositionRef(Vector3::ZERO)
        
    {

        // Reasonable defaults
        mAngle = 0;
        setDirection(Vector3::UNIT_X);
        mEmissionRate = 10;
        mMaxSpeed = mMinSpeed = 1;
        mMaxTTL = mMinTTL = 5;
        mPosition = Vector3::ZERO;
        mColourRangeStart = mColourRangeEnd = ColourValue::White;
        mEnabled = true;
        mRemainder = 0;
        mName = BLANKSTRING;
        mEmittedEmitter = BLANKSTRING;
        mEmitted = false;
    }
    //-----------------------------------------------------------------------
    ParticleEmitter::~ParticleEmitter() 
    = default;
    //-----------------------------------------------------------------------
    void ParticleEmitter::setPosition(const Vector3& pos) 
    { 
        mPosition = pos; 
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getPosition() const noexcept -> const Vector3& 
    { 
        return mPosition; 
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setDirection(const Vector3& inDirection) 
    { 
        mDirection = inDirection; 
        mDirection.normalise();
        // Generate a default up vector.
        mUp = mDirection.perpendicular();
        mUp.normalise();
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getDirection() const noexcept -> const Vector3&
    { 
        return mDirection; 
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setUp(const Vector3& inUp) 
    {
        mUp = inUp; 
        mUp.normalise();
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getUp() const noexcept -> const Vector3&
    { 
        return mUp; 
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setDirPositionReference( const Vector3& nposition, bool enable )
    { 
    mUseDirPositionRef  = enable;
        mDirPositionRef     = nposition;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getDirPositionReference() const noexcept -> const Vector3&
    {
        return mDirPositionRef;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getDirPositionReferenceEnabled() const noexcept -> bool
    {
        return mUseDirPositionRef;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setAngle(const Radian& angle)
    {
        // Store as radians for efficiency
        mAngle = angle;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getAngle() const noexcept -> const Radian&
    {
        return mAngle;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setParticleVelocity(Real speed)
    {
        assert(std::isfinite(speed));
        mMinSpeed = mMaxSpeed = speed;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setParticleVelocity(Real min, Real max)
    {
        assert(std::isfinite(min) && std::isfinite(max));
        mMinSpeed = min;
        mMaxSpeed = max;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setEmissionRate(Real particlesPerSecond) 
    { 
        mEmissionRate = particlesPerSecond; 
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getEmissionRate() const -> Real 
    { 
        return mEmissionRate; 
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setTimeToLive(Real ttl)
    {
        assert (ttl >= 0 && "Time to live can not be negative");
        mMinTTL = mMaxTTL = ttl;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setTimeToLive(Real minTtl, Real maxTtl)
    {
        assert (minTtl >= 0 && "Time to live can not be negative");
        assert (maxTtl >= 0 && "Time to live can not be negative");
        mMinTTL = minTtl;
        mMaxTTL = maxTtl;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setColour(const ColourValue& inColour)
    {
        mColourRangeStart = mColourRangeEnd = inColour;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setColour(const ColourValue& colourStart, const ColourValue& colourEnd)
    {
        mColourRangeStart = colourStart;
        mColourRangeEnd = colourEnd;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getName() const noexcept -> std::string_view
    {
        return mName;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setName(std::string_view newName)
    {
        mName = newName;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getEmittedEmitter() const noexcept -> std::string_view
    {
        return mEmittedEmitter;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setEmittedEmitter(std::string_view emittedEmitter)
    {
        mEmittedEmitter = emittedEmitter;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::isEmitted() const noexcept -> bool
    {
        return mEmitted;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setEmitted(bool emitted)
    {
        mEmitted = emitted;
    }
    //-----------------------------------------------------------------------
    static auto sampleSphereUniform(const float& maxAngle) -> float
    {
        float cosMax = -std::cos(maxAngle) + 1; // for maxAngle = pi, cosMax = 2
        // see https://corysimon.github.io/articles/uniformdistn-on-sphere/
        return std::acos(1 - cosMax * Math::UnitRandom());
    }

    void ParticleEmitter::genEmissionDirection( const Vector3 &particlePos, Vector3& destVector )
    {
        if( mUseDirPositionRef )
        {
            Vector3 particleDir = particlePos - mDirPositionRef;
            particleDir.normalise();

            if (mAngle != Radian{0})
            {
                // Randomise angle
                Radian angle{sampleSphereUniform(mAngle.valueRadians())};

                // Randomise direction
                destVector = particleDir.randomDeviant( angle );
            }
            else
            {
                // Constant angle
                destVector = particleDir;
            }
        }
        else
        {
            if (mAngle != Radian{0})
            {
                // Randomise angle
                Radian angle{sampleSphereUniform(mAngle.valueRadians())};

                // Randomise direction
                destVector = mDirection.randomDeviant(angle, mUp);
            }
            else
            {
                // Constant angle
                destVector = mDirection;
            }
        }

        // Don't normalise, we can assume that it will still be a unit vector since
        // both direction and 'up' are.
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::genEmissionVelocity(Vector3& destVector)
    {
        Real scalar;
        if (mMinSpeed != mMaxSpeed)
        {
            scalar = mMinSpeed + (Math::UnitRandom() * (mMaxSpeed - mMinSpeed));
        }
        else
        {
            scalar = mMinSpeed;
        }

        destVector *= scalar;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::genEmissionTTL() -> Real
    {
        if (mMaxTTL != mMinTTL)
        {
            return mMinTTL + (Math::UnitRandom() * (mMaxTTL - mMinTTL));
        }
        else
        {
            return mMinTTL;
        }
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::genConstantEmissionCount(Real timeElapsed) -> unsigned short
    {
        if (mEnabled)
        {
            if(mDurationMax < 0)
            {
                // single-shot burst
                setEnabled(false);
                return mEmissionRate;
            }
            // Keep fractions, otherwise a high frame rate will result in zero emissions!
            mRemainder += mEmissionRate * timeElapsed;
            auto intRequest = (unsigned short)mRemainder;
            mRemainder -= intRequest;

            // Check duration
            if (mDurationMax)
            {
                mDurationRemain -= timeElapsed;
                if (mDurationRemain <= 0) 
                {
                    // Disable, duration is out (takes effect next time)
                    setEnabled(false);
                }
            }
            return intRequest;
        }
        else
        {
            // Check repeat
            if (mRepeatDelayMax)
            {
                mRepeatDelayRemain -= timeElapsed;
                if (mRepeatDelayRemain <= 0)
                {
                    // Enable, repeat delay is out (takes effect next time)
                    setEnabled(true);
                }
            }
            if(mStartTime)
            {
                mStartTime -= timeElapsed;
                if(mStartTime <= 0)
                {
                    setEnabled(true);
                    mStartTime = 0;
                }
            }
            return 0;
        }

    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::genEmissionColour(RGBA& destColour)
    {
        if (mColourRangeStart != mColourRangeEnd)
        {
            // Randomise
            ColourValue t{Math::UnitRandom(), Math::UnitRandom(), Math::UnitRandom(), Math::UnitRandom()};
            destColour = (mColourRangeStart + t * (mColourRangeEnd - mColourRangeStart)).getAsBYTE();
        }
        else
        {
            destColour = mColourRangeStart.getAsBYTE();
        }
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::addBaseParameters()    
    {
        ParamDictionary* dict = getParamDictionary();

        dict->addParameter(ParameterDef("angle", 
            "The angle up to which particles may vary in their initial direction "
            "from the emitters direction, in degrees." , ParameterType::REAL),
            &msAngleCmd);

        dict->addParameter(ParameterDef("colour", 
            "The colour of emitted particles.", ParameterType::COLOURVALUE),
            &msColourCmd);

        dict->addParameter(ParameterDef("colour_range_start", 
            "The start of a range of colours to be assigned to emitted particles.", ParameterType::COLOURVALUE),
            &msColourRangeStartCmd);

        dict->addParameter(ParameterDef("colour_range_end", 
            "The end of a range of colours to be assigned to emitted particles.", ParameterType::COLOURVALUE),
            &msColourRangeEndCmd);

        dict->addParameter(ParameterDef("direction", 
            "The base direction of the emitter." , ParameterType::VECTOR3),
            &msDirectionCmd);

        dict->addParameter(ParameterDef("up", 
            "The up vector of the emitter." , ParameterType::VECTOR3),
            &msUpCmd);

        dict->addParameter(ParameterDef("direction_position_reference", 
            "The reference position to calculate the direction of emitted particles "
            "based on their position. Good for explosions and implosions (use negative velocity)" , ParameterType::COLOURVALUE),
            &msDirPositionRefCmd);

        dict->addParameter(ParameterDef("emission_rate", 
            "The number of particles emitted per second." , ParameterType::REAL),
            &msEmissionRateCmd);

        dict->addParameter(ParameterDef("position", 
            "The position of the emitter relative to the particle system center." , ParameterType::VECTOR3),
            &msPositionCmd);

        dict->addParameter(ParameterDef("velocity", 
            "The initial velocity to be assigned to every particle, in world units per second." , ParameterType::REAL),
            &msVelocityCmd);

        dict->addParameter(ParameterDef("velocity_min", 
            "The minimum initial velocity to be assigned to each particle." , ParameterType::REAL),
            &msMinVelocityCmd);

        dict->addParameter(ParameterDef("velocity_max", 
            "The maximum initial velocity to be assigned to each particle." , ParameterType::REAL),
            &msMaxVelocityCmd);

        dict->addParameter(ParameterDef("time_to_live", 
            "The lifetime of each particle in seconds." , ParameterType::REAL),
            &msTTLCmd);

        dict->addParameter(ParameterDef("time_to_live_min", 
            "The minimum lifetime of each particle in seconds." , ParameterType::REAL),
            &msMinTTLCmd);

        dict->addParameter(ParameterDef("time_to_live_max", 
            "The maximum lifetime of each particle in seconds." , ParameterType::REAL),
            &msMaxTTLCmd);

        dict->addParameter(ParameterDef("duration", 
            "The length of time in seconds which an emitter stays enabled for." , ParameterType::REAL),
            &msDurationCmd);

        dict->addParameter(ParameterDef("duration_min", 
            "The minimum length of time in seconds which an emitter stays enabled for." , ParameterType::REAL),
            &msMinDurationCmd);

        dict->addParameter(ParameterDef("duration_max", 
            "The maximum length of time in seconds which an emitter stays enabled for." , ParameterType::REAL),
            &msMaxDurationCmd);

        dict->addParameter(ParameterDef("repeat_delay", 
            "If set, after disabling an emitter will repeat (reenable) after this many seconds." , ParameterType::REAL),
            &msRepeatDelayCmd);

        dict->addParameter(ParameterDef("repeat_delay_min", 
            "If set, after disabling an emitter will repeat (reenable) after this minimum number of seconds." , ParameterType::REAL),
            &msMinRepeatDelayCmd);

        dict->addParameter(ParameterDef("repeat_delay_max", 
            "If set, after disabling an emitter will repeat (reenable) after this maximum number of seconds." , ParameterType::REAL),
            &msMaxRepeatDelayCmd);

        dict->addParameter(ParameterDef("name", 
            "This is the name of the emitter" , ParameterType::STRING),
            &msNameCmd);
        
        dict->addParameter(ParameterDef("emit_emitter", 
            "If set, this emitter will emit other emitters instead of visual particles" , ParameterType::STRING),
            &msEmittedEmitterCmd);
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getParticleVelocity() const -> Real
    {
        return mMinSpeed;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getMinParticleVelocity() const -> Real
    {
        return mMinSpeed;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getMaxParticleVelocity() const -> Real
    {
        return mMaxSpeed;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setMinParticleVelocity(Real min)
    {
        assert(std::isfinite(min));
        mMinSpeed = min;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setMaxParticleVelocity(Real max)
    {
        assert(std::isfinite(max));
        mMaxSpeed = max;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getTimeToLive() const -> Real
    {
        return mMinTTL;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getMinTimeToLive() const -> Real
    {
        return mMinTTL;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getMaxTimeToLive() const -> Real
    {
        return mMaxTTL;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setMinTimeToLive(Real min)
    {
        mMinTTL = min;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setMaxTimeToLive(Real max)
    {
        mMaxTTL = max;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getColour() const noexcept -> const ColourValue&
    {
        return mColourRangeStart;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getColourRangeStart() const noexcept -> const ColourValue&
    {
        return mColourRangeStart;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getColourRangeEnd() const noexcept -> const ColourValue&
    {
        return mColourRangeEnd;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setColourRangeStart(const ColourValue& val)
    {
        mColourRangeStart = val;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setColourRangeEnd(const ColourValue& val )
    {
        mColourRangeEnd = val;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setEnabled(bool enabled)
    {
        mEnabled = enabled;
        // Reset duration & repeat
        initDurationRepeat();
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getEnabled() const noexcept -> bool
    {
        return mEnabled;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setStartTime(Real startTime)
    {
        setEnabled(false);
        mStartTime = startTime;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getStartTime() const -> Real
    {
        return mStartTime;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setDuration(Real duration)
    {
        setDuration(duration, duration);
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getDuration() const -> Real
    {
        return mDurationMin;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setDuration(Real min, Real max)
    {
        mDurationMin = min;
        mDurationMax = max;
        initDurationRepeat();
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setMinDuration(Real min)
    {
        mDurationMin = min;
        initDurationRepeat();
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setMaxDuration(Real max)
    {
        mDurationMax = max;
        initDurationRepeat();
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::initDurationRepeat()
    {
        if (mEnabled)
        {
            if (mDurationMin == mDurationMax)
            {
                mDurationRemain = mDurationMin;
            }
            else
            {
                mDurationRemain = Math::RangeRandom(mDurationMin, mDurationMax);
            }
        }
        else
        {
            // Reset repeat
            if (mRepeatDelayMin == mRepeatDelayMax)
            {
                mRepeatDelayRemain = mRepeatDelayMin;
            }
            else
            {
                mRepeatDelayRemain = Math::RangeRandom(mRepeatDelayMax, mRepeatDelayMin);
            }

        }
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setRepeatDelay(Real delay)
    {
        setRepeatDelay(delay, delay);
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getRepeatDelay() const -> Real
    {
        return mRepeatDelayMin;
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setRepeatDelay(Real min, Real max)
    {
        mRepeatDelayMin = min;
        mRepeatDelayMax = max;
        initDurationRepeat();
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setMinRepeatDelay(Real min)
    {
        mRepeatDelayMin = min;
        initDurationRepeat();
    }
    //-----------------------------------------------------------------------
    void ParticleEmitter::setMaxRepeatDelay(Real max)
    {
        mRepeatDelayMax = max;
        initDurationRepeat();
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getMinDuration() const -> Real
    {
        return mDurationMin;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getMaxDuration() const -> Real
    {
        return mDurationMax;
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getMinRepeatDelay() const -> Real
    {
        return mRepeatDelayMin;    
    }
    //-----------------------------------------------------------------------
    auto ParticleEmitter::getMaxRepeatDelay() const -> Real
    {
        return mRepeatDelayMax;    
    }

    //-----------------------------------------------------------------------
    ParticleEmitterFactory::~ParticleEmitterFactory()
    {
        // Destroy all emitters
        for (auto & mEmitter : mEmitters)
        {
            delete mEmitter;
        }
            
        mEmitters.clear();

    }
    //-----------------------------------------------------------------------
    void ParticleEmitterFactory::destroyEmitter(ParticleEmitter* e)        
    {
        for (auto i = mEmitters.begin(); i != mEmitters.end(); ++i)
        {
            if ((*i) == e)
            {
                mEmitters.erase(i);
                delete e;
                break;
            }
        }
    }

    //-----------------------------------------------------------------------

}
