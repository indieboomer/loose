#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include "physics.hpp"
#include <thread>
#include <mutex>
#include <stdexcept>
namespace loose {
namespace {
JPH::Vec3 j(vec3 v){return {v.x,v.y,v.z};}
JPH::Quat j(quat q){return {q.x,q.y,q.z,q.w};}
vec3 g(JPH::Vec3 v){return {v.GetX(),v.GetY(),v.GetZ()};}
quat g(JPH::Quat q){return {q.GetW(),q.GetX(),q.GetY(),q.GetZ()};}
struct Runtime {
    Runtime(){JPH::RegisterDefaultAllocator();JPH::Factory::sInstance=new JPH::Factory;JPH::RegisterTypes();}
    ~Runtime(){JPH::UnregisterTypes();delete JPH::Factory::sInstance;JPH::Factory::sInstance=nullptr;}
};
Runtime& runtime(){static Runtime r;return r;}
struct BroadLayers : JPH::BroadPhaseLayerInterface {
    JPH::uint GetNumBroadPhaseLayers()const override{return 2;}
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer)const override{return JPH::BroadPhaseLayer(layer==0?0:1);}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer l)const override{return l.GetValue()==0?"Static":"Moving";}
#endif
};
struct PairFilter : JPH::ObjectLayerPairFilter {
    bool ShouldCollide(JPH::ObjectLayer a,JPH::ObjectLayer b)const override{return (a==1||b==1);}
};
struct BroadFilter : JPH::ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer a,JPH::BroadPhaseLayer b)const override{return a==1||b.GetValue()==1;}
};
struct QueryFilter : JPH::ObjectLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer l)const override{return l!=2;}
};
}
struct Physics::Impl {
    Level level;
    BroadLayers broad;PairFilter pairs;BroadFilter filter;QueryFilter query;
    JPH::PhysicsSystem system;
    JPH::TempAllocatorImpl temp{16*1024*1024};
    JPH::JobSystemThreadPool jobs{JPH::cMaxPhysicsJobs,JPH::cMaxPhysicsBarriers,2};
    JPH::RefConst<JPH::Shape> capsule;
    JPH::Ref<JPH::CharacterVirtual> character;
    JPH::BodyID cubeID;
    std::vector<JPH::BodyID> bodies;
    quat roomRotation{1,0,0,0};
    vec3 roomOffset{};
    vec3 toWorld(vec3 v)const{return roomRotation*v+roomOffset;}
    vec3 toRoom(vec3 v)const{return glm::conjugate(roomRotation)*(v-roomOffset);}
    explicit Impl(const Level& l):level(l) {
        system.Init(128,0,256,256,broad,filter,pairs);
        auto& bi=system.GetBodyInterface();
        for(auto& b:l.boxes){
            JPH::BodyCreationSettings s(new JPH::BoxShape(j(b.half),.01f),j(b.center),JPH::Quat::sIdentity(),JPH::EMotionType::Static,0);
            s.mFriction=.6f;
            bodies.push_back(bi.CreateAndAddBody(s,JPH::EActivation::DontActivate));
        }
        JPH::BodyCreationSettings s(new JPH::BoxShape(JPH::Vec3::sReplicate(l.cubeSide*.5f),.015f),j(l.cubeSpawn),JPH::Quat::sIdentity(),JPH::EMotionType::Dynamic,1);
        s.mFriction=.6f;s.mRestitution=.05f;s.mLinearDamping=.05f;s.mAngularDamping=.08f;
        s.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;
        s.mMassPropertiesOverride.mMass=30.f;s.mMotionQuality=JPH::EMotionQuality::LinearCast;
        cubeID=bi.CreateAndAddBody(s,JPH::EActivation::Activate);bodies.push_back(cubeID);
        capsule=new JPH::CapsuleShape(l.player.height*.5f-l.player.radius,l.player.radius);
        createCharacter(l.spawnFeet+vec3(0,l.player.height*.5f,0),quat(1,0,0,0),vec3(0,1,0));
        system.OptimizeBroadPhase();
    }
    void createCharacter(vec3 center,quat frame,vec3 up){
        // Recreating only this tiny controller clears its old contact and grounding cache.
        character=nullptr;
        JPH::CharacterVirtualSettings s;s.mShape=capsule;s.mUp=j(up);
        // 60 degrees admits a support face even when gravity points toward a 3-axis corner.
        s.mMaxSlopeAngle=glm::radians(60.f);s.mMaxStrength=250.f;s.mMass=70.f;
        s.mSupportingVolume=JPH::Plane(JPH::Vec3::sAxisY(),-level.player.radius);
        s.mInnerBodyShape=capsule;s.mInnerBodyLayer=2;
        character=new JPH::CharacterVirtual(&s,j(center),j(frame),0,&system);
    }
    ~Impl(){
        character=nullptr;
        auto& bi=system.GetBodyInterface();
        for(auto id:bodies){bi.RemoveBody(id);bi.DestroyBody(id);}
    }
};
Physics::Physics(const Level& l){(void)runtime();p=std::make_unique<Impl>(l);}
Physics::~Physics()=default;
void Physics::reset(){
    p->roomRotation=quat(1,0,0,0);p->roomOffset={};
    auto& bi=p->system.GetBodyInterface();
    for(size_t i=0;i<p->level.boxes.size();i++)bi.SetPositionAndRotation(p->bodies[i],j(p->level.boxes[i].center),JPH::Quat::sIdentity(),JPH::EActivation::DontActivate);
    p->system.SetGravity({0,-gravityStrength,0});
    placeCube({p->level.cubeSpawn,quat(1,0,0,0)});
    p->createCharacter(p->level.spawnFeet+vec3(0,p->level.player.height*.5f,0),quat(1,0,0,0),{0,1,0});
}
void Physics::step(float dt,vec3 wish,bool jump,vec3 up){
    wish=p->roomRotation*wish;up=p->roomRotation*up;
    auto& c=*p->character;
    vec3 v=g(c.GetLinearVelocity());
    float vertical=glm::dot(v,up);
    if(grounded()&&vertical<=.1f){vertical=0;if(jump)vertical=p->level.player.jump;}
    v=wish*p->level.player.speed+vertical*up-gravityStrength*up*dt;
    c.SetLinearVelocity(j(v)); // ExtendedUpdate's gravity parameter does NOT accelerate this velocity.
    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mStickToFloorStepDown=jump?JPH::Vec3::sZero():j(-up*.25f);
    settings.mWalkStairsStepUp=j(up*.3f);
    settings.mWalkStairsStepDownExtra=j(-up*.02f);
    c.ExtendedUpdate(dt,j(-gravityStrength*up),settings,{},p->query,{},{},p->temp);
    auto result=p->system.Update(dt,1,&p->temp,&p->jobs);
    if(result!=JPH::EPhysicsUpdateError::None)throw std::runtime_error("Jolt fixed-step capacity exceeded");
}
void Physics::commitOrientation(vec3 anchoredEye,vec3 up,quat frame){
    // Rotate geometry into the fixed-gravity physics frame, preserving inertial velocities.
    vec3 worldEye=p->toWorld(anchoredEye);
    vec3 newUp=glm::normalize(p->roomRotation*up);
    quat delta=glm::rotation(newUp,vec3(0,1,0));
    auto& bi=p->system.GetBodyInterface();
    for(auto id:p->bodies){
        vec3 position=g(bi.GetPosition(id));quat rotation=g(bi.GetRotation(id));
        bi.SetPositionAndRotation(id,j(worldEye+delta*(position-worldEye)),j(glm::normalize(delta*rotation)),JPH::EActivation::DontActivate);
        bi.InvalidateContactCache(id);
    }
    p->roomOffset=worldEye+delta*(p->roomOffset-worldEye);
    p->roomRotation=glm::normalize(delta*p->roomRotation);
    p->createCharacter(p->toWorld(anchoredEye-up*(p->level.player.eyeHeight-p->level.player.height*.5f)),glm::normalize(p->roomRotation*frame),{0,1,0});
    bi.ActivateBody(p->cubeID);
}
bool Physics::rotationClear(const Orientation& proposed)const {
    const auto& cfg=p->level.player;
    constexpr int samples=180; // Half-degree intervals, endpoint included.
    // A point on the capsule moves at most 1.35m * half an interval = 5.9mm.
    // Inflation encloses the unsampled arcs; tolerance admits only negligible contacts.
    constexpr float inflation=.007f,tolerance=.003f;
    JPH::RefConst<JPH::Shape> swept=new JPH::CapsuleShape(cfg.height*.5f-cfg.radius,cfg.radius+inflation);
    JPH::CollideShapeSettings settings;settings.mCollisionTolerance=.0005f;
    for(int i=0;i<=samples;i++){
        quat q=proposed.at(float(i)/samples);vec3 u=q*vec3(0,1,0);
        vec3 center=proposed.eyeAnchor-u*(cfg.eyeHeight-cfg.height*.5f)+u*.02f;
        center=p->toWorld(center);q=glm::normalize(p->roomRotation*q);
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
        p->system.GetNarrowPhaseQuery().CollideShape(swept,JPH::Vec3::sReplicate(1),JPH::RMat44::sRotationTranslation(j(q),j(center)),settings,JPH::RVec3::sZero(),hits,{},p->query);
        for(auto& hit:hits.mHits)if(hit.mPenetrationDepth>tolerance)return false;
    }
    return true;
}
vec3 Physics::center()const{return p->toRoom(g(p->character->GetPosition()));}
vec3 Physics::eye(vec3 up)const{return center()+up*(p->level.player.eyeHeight-p->level.player.height*.5f);}
vec3 Physics::velocity()const{return glm::conjugate(p->roomRotation)*g(p->character->GetLinearVelocity());}
Pose Physics::cube()const{auto& b=p->system.GetBodyInterface();return {p->toRoom(g(b.GetPosition(p->cubeID))),glm::normalize(glm::conjugate(p->roomRotation)*g(b.GetRotation(p->cubeID)))};}
vec3 Physics::cubeVelocity()const{return glm::conjugate(p->roomRotation)*g(p->system.GetBodyInterface().GetLinearVelocity(p->cubeID));}
vec3 Physics::cubeAngularVelocity()const{return glm::conjugate(p->roomRotation)*g(p->system.GetBodyInterface().GetAngularVelocity(p->cubeID));}
bool Physics::cubeAwake()const{return p->system.GetBodyInterface().IsActive(p->cubeID);}
bool Physics::grounded()const{return p->character->GetGroundState()==JPH::CharacterBase::EGroundState::OnGround;}
void Physics::placePlayer(vec3 c,quat q,vec3 up){p->createCharacter(p->toWorld(c),glm::normalize(p->roomRotation*q),p->roomRotation*up);}
void Physics::placeCube(Pose pose,vec3 velocity,vec3 angularVelocity){
    auto& bi=p->system.GetBodyInterface();
    bi.SetPositionAndRotation(p->cubeID,j(p->toWorld(pose.position)),j(glm::normalize(p->roomRotation*pose.rotation)),JPH::EActivation::Activate);
    bi.SetLinearAndAngularVelocity(p->cubeID,j(p->roomRotation*velocity),j(p->roomRotation*angularVelocity));bi.ResetSleepTimer(p->cubeID);
    bi.InvalidateContactCache(p->cubeID);
}
}
