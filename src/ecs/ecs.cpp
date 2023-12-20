#include "ecs.hpp"
#include <cassert>

int BaseComponent::nextId = 0;

// ----------------------
// ENTITY
// ----------------------

int Entity::GetId() const
{
	return id;
}

void Entity::Kill() {
	registry->KillEntity(*this);
}

// tag management
void Entity::Tag(const std::string& tag) {
	registry->TagEntity(*this, tag);
}
bool Entity::HasTag(const std::string& tag) const {
	return registry->EntityhasTag(*this, tag);
}
void Entity::Group(const std::string& groupName) {
	registry->GroupEntity(*this, groupName);
}
bool Entity::BelongsToGroup(const std::string& groupName) const {
	return registry->EntityBelongsToGroup(*this, groupName);
}


// ----------------------
// SYSTEM
// ----------------------
// should we check if the entity does not already exist?
void System::AddEntityToSystem(Entity entity) {
	entities.push_back(entity);
}

void System::RemoveEntityFromSystem(Entity entity) {
	assert(false && "this is disabled for a jolt test. please re-enable me.");
	// std::erase(entities, entity);
}
std::vector<Entity> System::GetSystemEntities() const
{
	return entities;
}
const Signature& System::GetComponentSignature() const
{
	return componentSignature;
}


// REGISTRY
Entity Registry::CreateEntity()
{
	int entityId;
	if (freeIds.empty())
	{
		entityId = numEntities++;
		if (entityId >= entityComponentSignatures.size()) {
			entityComponentSignatures.resize(entityId + 1);
		}
	}
	else {
		// reuse an id from the list of previously removed entities.
		entityId = freeIds.front();
		freeIds.pop_front();
	}

	Entity entity(entityId, this);
	entitiesToBeAdded.insert(entity);

	// Logger::Log("Entity created with id = " + std::to_string(entityId));

	return entity;
}

void Registry::KillEntity(Entity entity) {
	entitiesToBeKilled.insert(entity);
	// Logger::Log("Entity " + std::to_string(entity.GetId()) + " was killed");
}


void Registry::AddEntityToSystems(Entity entity) {
	const auto entityid = entity.GetId();
	
	auto entityComponentSignature = entityComponentSignatures[entityid];
	
	for (auto& [key, system]: systems)
	{
		const auto& systemComponentSignature = system->GetComponentSignature();
		
		// bitwise and, see if results are identical to system component signature.
		bool isInterested = (entityComponentSignature & systemComponentSignature) == systemComponentSignature;

		if (isInterested)
		{
			// todo add the entity to the system.
			system->AddEntityToSystem(entity);
		}
	}
}

void Registry::RemoveEntityFromSystems(Entity entity) {
	for (auto [system_name, system] : systems) {
		system->RemoveEntityFromSystem(entity);
	}
}

// tag management
void Registry::TagEntity(Entity entity, const std::string& tag) {
	entityPerTag.emplace(tag, entity);
	tagPerEntity.emplace(entity.GetId(), tag);
}
bool Registry::EntityhasTag(Entity entity, const std::string& tag) const {
	if (tagPerEntity.find(entity.GetId()) == tagPerEntity.end()) {
		return false;
	}

	// uh, doing this twice? let's hope the compiler optimizes! :~)
	return entityPerTag.find(tag)->second == entity;

}
Entity Registry::GetEntityByTag(const std::string& tag) const {
	// huh, what if this does not exist? ah , we use .at() which will throw. sigh.
	return entityPerTag.at(tag);
}
void Registry::RemoveEntityTag(Entity entity) {
	auto taggedEntity = tagPerEntity.find(entity.GetId());
	if (taggedEntity != tagPerEntity.end()) {
		auto tag = taggedEntity->second;
		entityPerTag.erase(tag);
		tagPerEntity.erase(taggedEntity);
	}

}

// group management
void Registry::GroupEntity(Entity entity, const std::string& group) {
	entitiesPerGroup.emplace(group, std::set<Entity>());
	entitiesPerGroup[group].emplace(entity);
	groupPerEntity.emplace(entity.GetId(), group);
}
bool Registry::EntityBelongsToGroup(Entity entity, const std::string& group) const {
	if (groupPerEntity.find(entity.GetId()) == groupPerEntity.end()) {
		return false;
	}
	return groupPerEntity.find(entity.GetId())->second == group;

}
std::vector<Entity> Registry::GetEntitiesByGroup(const std::string& group) const {
	auto& setOfEntities = entitiesPerGroup.at(group);
	return std::vector<Entity>(setOfEntities.begin(), setOfEntities.end());
}
void Registry::RemoveEntityGroup(Entity entity) {
	// if in group, remove entity from group management.
	auto groupedEntity = groupPerEntity.find(entity.GetId());
	if (groupedEntity != groupPerEntity.end()) {
		auto group = entitiesPerGroup.find(groupedEntity->second);
		if (group != entitiesPerGroup.end()) {
			auto entityInGroup = group->second.find(entity);
			if (entityInGroup != group->second.end()) {
				group->second.erase(entityInGroup);
			}
		}
		groupPerEntity.erase(groupedEntity);
	}
}




//
void Registry::Update()
{
	// at the end of the frame, actually kill entities and spawn new ones.
	for (auto entity : entitiesToBeAdded)
	{
		AddEntityToSystems(entity);
	}
	entitiesToBeAdded.clear();

	// process the entities that are waiting to be killed from the active systems.
	for (auto entity : entitiesToBeKilled) {
		RemoveEntityFromSystems(entity);
		entityComponentSignatures[entity.GetId()].reset();

		// rewmove the entity from the component pools.
		for (auto pool : componentPools) {
			if (pool) {
				pool->RemoveEntityFromPool(entity.GetId());
			}
		}

		// make the entity id available to be reused.
		freeIds.push_back(entity.GetId());

		// remove any trace existence of that entity from the tag / group maps
		RemoveEntityTag(entity);
		RemoveEntityGroup(entity);
	}
	entitiesToBeKilled.clear();
}