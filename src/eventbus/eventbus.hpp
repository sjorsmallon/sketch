#pragma once

#include "../logger/logger.hpp"
#include <typeindex>
#include <map>
#include <list>
#include <memory>
#include "event.hpp"

class IEventCallback {
private:
	virtual void Call(Event& e) = 0;
public: 
	virtual ~IEventCallback() = default;

	void Execute(Event& e) {
		Call(e);
	}
};
// wrapper around a function pointer. e.g. EventCallback<MouseMoveEvent>
// this contains a function pointer of a specific member function of an object.
// we need to have the "owner" since a pointer to a member function is initially
// just an offset from the start of that object to that specific function.
template <typename TOwner, typename TEvent>
class EventCallback : public IEventCallback {
private:
	typedef void (TOwner::* CallbackFunction)(TEvent&);

	TOwner* ownerInstance;
	CallbackFunction callbackFunction;

	virtual void Call(Event& e) override {
		std::invoke(callbackFunction, ownerInstance, static_cast<TEvent&>(e));
	}

public:
	EventCallback(TOwner* ownerInstance, CallbackFunction callbackFunction) {
		this->ownerInstance = ownerInstance;
		this->callbackFunction = callbackFunction;
	}

	virtual ~EventCallback() override = default;
};
// list of unique callback functions.
typedef std::list<std::unique_ptr<IEventCallback>> HandlerList;

class EventBus {
private: 

	std::map<std::type_index, std::unique_ptr<HandlerList>> subscribers;

public:
	EventBus() = default;
	~EventBus() = default;


	// clear the subscriber list.
	void Reset() {
		subscribers.clear();
	}


	// subscribe to an event type <Tevent>
	// in our implementation, a listener subscribes to an event.
	// exmaple./eventBus->SubscribeToEvent<CollisionEvent>(&Game::onCollision);
	template <typename TEvent, typename TOwner>
	void SubscribeToEvent(TOwner* ownerInstance, void (TOwner::*callbackFunction)(TEvent&)) {

		if (!subscribers[typeid(TEvent)].get())
		{
			subscribers[typeid(TEvent)] = std::make_unique<HandlerList>();
		}
		auto subscriber = std::make_unique<EventCallback<TOwner, TEvent>>(ownerInstance, callbackFunction);
		subscribers[typeid(TEvent)]->push_back(std::move(subscriber));
	}

	// emit an event of type <TEvent>
	// as soon as something emits an event we go ahead and execute all the listener
	// callback functions.
	//example: eventbus->EmitEvent<CollisionEvent>(player, enemy);
	template <typename TEvent, typename ...TArgs>
	void EmitEvent(TArgs&& ...args) {
		auto handlers = subscribers[typeid(TEvent)].get();
		if (handlers) {
			for (auto it = handlers->begin(); it != handlers->end(); it++) {
				auto handler = it->get();
				TEvent event(std::forward<TArgs>(args)...);
				handler->Execute(event);
			}
		}
	}
};