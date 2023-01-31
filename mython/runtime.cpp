#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (!object) {
        return false;
    }

    if (auto obj = object.TryAs<Bool>()) {
        return obj->GetValue() == true;
    }
    if (auto obj = object.TryAs<Number>()) {
        return !(obj->GetValue() == 0);
    }
    if (auto obj = object.TryAs<String>()) {
        return !(obj->GetValue().empty());
    }
    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (this->HasMethod("__str__", 0)) {
        this->Call("__str__", {}, context)->Print(os, context);
    } else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    return (cls_.GetMethod(method) && cls_.GetMethod(method)->formal_params.size() == argument_count);
}

Closure& ClassInstance::Fields() {
    return fields_;
}

const Closure& ClassInstance::Fields() const {
    return fields_;
}

ClassInstance::ClassInstance(const Class& cls): cls_(cls) {
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if (!this->HasMethod(method, actual_args.size())) {
        throw std::runtime_error("Not implemented"s);
    }
    
    Closure closure = { {"self", ObjectHolder::Share(*this)} };
    auto method_ptr = cls_.GetMethod(method);
    
    for (size_t i = 0; i < method_ptr->formal_params.size(); ++i) {
        std::string arg = method_ptr->formal_params[i];
        closure[arg] = actual_args[i];
    }
    
    return method_ptr->body->Execute(closure, context);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent): name_(std::move(name)),
                                                                                  methods_(std::move(methods)),
                                                                                  parent_(std::move(parent)) {
    if (parent_ != nullptr) {
        for (const auto& method : parent_->methods_) {
            names_methods_[method.name] = &method;
        }
    }
                                                                                      
    for (const auto& method : methods_) {
        names_methods_[method.name] = &method;
    }
}

const Method* Class::GetMethod(const std::string& name) const {
    if (names_methods_.count(name) != 0) {
        return names_methods_.at(name);
    }
    return nullptr;
}

const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "s << GetName();
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
        return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
    }
    if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
    }
    if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
    }
    if (!lhs && !rhs) {
        return true;
    }
    if (lhs.TryAs<ClassInstance>() && lhs.TryAs<ClassInstance>()->HasMethod("__eq__", 1)) {
        return lhs.TryAs<ClassInstance>()
            ->Call("__eq__", {rhs}, context)
            .TryAs<Bool>()
            ->GetValue();
    }
    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
        return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
    }
    if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
    }
    if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
    }
    if (lhs.TryAs<ClassInstance>() && lhs.TryAs<ClassInstance>()->HasMethod("__lt__", 1)) {
        return lhs.TryAs<ClassInstance>()
            ->Call("__lt__", {rhs}, context)
            .TryAs<Bool>()
            ->GetValue();
    }
    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Greater(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}
}  // namespace runtime