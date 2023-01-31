#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = std::move(rv_->Execute(closure, context));
    return closure.at(var_);
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv): var_(std::move(var)), rv_(std::move(rv)) {
}

VariableValue::VariableValue(const std::string& var_name) {
    dotted_ids_.push_back(var_name);
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids): dotted_ids_(std::move(dotted_ids)) {
}

ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    if (dotted_ids_.size() > 0) {
        runtime::ObjectHolder result;
        Closure* current_closure_ptr = &closure;
        for (const auto& var_name : dotted_ids_) {
            auto var_it = current_closure_ptr->find(var_name);
            if (var_it == current_closure_ptr->end()) {
                throw std::runtime_error("Invalid argument name"s);
            }
            result = var_it->second;
            auto next_dotted_var = result.TryAs<runtime::ClassInstance>();
            if (next_dotted_var) {
                current_closure_ptr = &next_dotted_var->Fields();
            }
        }
        return result;
    }

    throw std::runtime_error("No arguments specified"s);
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return std::make_unique<Print>(std::make_unique<VariableValue>(name));
}

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args): args_(std::move(args)) {
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    runtime::ObjectHolder result;

    for (const auto& arg : args_) {
        if (arg != args_.front()) {
            context.GetOutputStream() << " "s;
        }
        
        result = arg->Execute(closure, context);
        
        if (result) {
            result->Print(context.GetOutputStream(), context);
        } else {
            context.GetOutputStream() << "None"s;
        }
    }
    context.GetOutputStream() << std::endl;
    return result;
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args): object_(std::move(object)),
                                                                      method_(std::move(method)),
                                                                      args_(std::move(args)) {
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    if (!object_) {
        return runtime::ObjectHolder::None();
    }
    auto obj = object_->Execute(closure, context);
    auto obj_ptr = obj.TryAs<runtime::ClassInstance>();
    std::vector<runtime::ObjectHolder> args_values;

    for (const auto& arg : args_) {
        args_values.push_back(std::move(arg->Execute(closure, context)));
    }
    
    auto result = obj_ptr->Call(method_, args_values, context);
    return result;
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    auto obj = argument_->Execute(closure, context);
    if (!obj) {
        return ObjectHolder::Own(runtime::String{ "None"s });
    }
    runtime::DummyContext dummy_context;
    obj->Print(dummy_context.GetOutputStream(), dummy_context);
    return ObjectHolder::Own(runtime::String{ dummy_context.output.str() });
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
    }
    
    auto lhs_res = lhs_->Execute(closure, context);
    auto rhs_res = rhs_->Execute(closure, context);

    auto lhs_num = lhs_res.TryAs<runtime::Number>();
    auto rhs_num = rhs_res.TryAs<runtime::Number>();
    
    if (lhs_num && rhs_num) {
        return ObjectHolder::Own(runtime::Number{lhs_num->GetValue() + rhs_num->GetValue()});
    }
    
    auto lhs_str = lhs_res.TryAs<runtime::String>();
    auto rhs_str = rhs_res.TryAs<runtime::String>();
    if (lhs_str && rhs_str) {
        return ObjectHolder::Own(runtime::String{lhs_str->GetValue() + rhs_str->GetValue()});
    }

    auto lhs_class_inst = lhs_res.TryAs<runtime::ClassInstance>();

    if (lhs_class_inst != nullptr) {
        const int ADD_METHOD_ARGS_COUNT = 1;

        if (lhs_class_inst->HasMethod(ADD_METHOD, ADD_METHOD_ARGS_COUNT)) {
            return lhs_class_inst->Call(ADD_METHOD, { rhs_res }, context);
        }
    }

    throw std::runtime_error("Wrong types for add operation");
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
   if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
    }
    
    auto lhs_res = lhs_->Execute(closure, context);
    auto rhs_res = rhs_->Execute(closure, context);

    auto lhs_num = lhs_res.TryAs<runtime::Number>();
    auto rhs_num = rhs_res.TryAs<runtime::Number>();
    
    if (lhs_num && rhs_num) {
        return ObjectHolder::Own(runtime::Number{lhs_num->GetValue() - rhs_num->GetValue()});
    }

    throw std::runtime_error("Wrong types for sub operation");
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
    }
    
    auto lhs_res = lhs_->Execute(closure, context);
    auto rhs_res = rhs_->Execute(closure, context);

    auto lhs_num = lhs_res.TryAs<runtime::Number>();
    auto rhs_num = rhs_res.TryAs<runtime::Number>();
    
    if (lhs_num && rhs_num) {
        return ObjectHolder::Own(runtime::Number{lhs_num->GetValue() * rhs_num->GetValue()});
    }

    throw std::runtime_error("Wrong types for mult operation");
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
   if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
    }
    
    auto lhs_res = lhs_->Execute(closure, context);
    auto rhs_res = rhs_->Execute(closure, context);

    auto lhs_num = lhs_res.TryAs<runtime::Number>();
    auto rhs_num = rhs_res.TryAs<runtime::Number>();
    
    if (lhs_num && rhs_num) {
        if(rhs_num->GetValue() == 0) {
            throw std::runtime_error("Division by zero");
        }
        return ObjectHolder::Own(runtime::Number{lhs_num->GetValue() / rhs_num->GetValue()});
    }

    throw std::runtime_error("Wrong types for div operation");
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (const auto& statement : stmt_) {
        statement->Execute(closure, context);
    }
    return runtime::ObjectHolder::None();
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw statement_->Execute(closure, context);
}

ClassDefinition::ClassDefinition(ObjectHolder cls): cls_(std::move(cls)) {
}

ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    auto obj = cls_.TryAs<runtime::Class>();
    closure[obj->GetName()] = std::move(cls_);
    return runtime::ObjectHolder::None();
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv): object_(std::move(object)),
                                                                 field_name_(std::move(field_name)),
                                                                 rv_(std::move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    auto obj = object_.Execute(closure, context);
    auto obj_ptr = obj.TryAs<runtime::ClassInstance>();
    obj_ptr->Fields()[field_name_] = std::move(rv_->Execute(closure, context));
    return obj_ptr->Fields().at(field_name_);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body): condition_(std::move(condition)),
                                                      if_body_(std::move(if_body)),
                                                      else_body_(std::move(else_body)) {
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    auto condition_result = condition_->Execute(closure, context);

    if (runtime::IsTrue(condition_result)) {
        return if_body_->Execute(closure, context);
    } else if (else_body_) {
        return else_body_->Execute(closure, context);
    }
    
    return runtime::ObjectHolder::None();
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
    }
    
    auto lhs_res = lhs_->Execute(closure, context);
    auto rhs_res = rhs_->Execute(closure, context);
    
    if (runtime::IsTrue(lhs_res)) {
        return ObjectHolder::Own(runtime::Bool{ true });
    }
    if (runtime::IsTrue(rhs_res)) {
        return ObjectHolder::Own(runtime::Bool{ true });
    }

    return ObjectHolder::Own(runtime::Bool{ false });
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
    }
    
    auto lhs_res = lhs_->Execute(closure, context);
    auto rhs_res = rhs_->Execute(closure, context);
    
    if (runtime::IsTrue(lhs_res) && runtime::IsTrue(rhs_res)) {
        return ObjectHolder::Own(runtime::Bool{ true });
    }
    
    return ObjectHolder::Own(runtime::Bool{ false });
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    if (!argument_) {
        throw std::runtime_error("null operands are not supported"s);
    }
    
    auto obj = argument_->Execute(closure, context);
    auto res = runtime::IsTrue(obj);
    
    return ObjectHolder::Own(runtime::Bool{ !res });
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(std::move(cmp)) {
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
    }
    
    auto lhs_res = lhs_->Execute(closure, context);
    auto rhs_res = rhs_->Execute(closure, context);
    bool res = cmp_(lhs_res, rhs_res, context);
    
    return ObjectHolder::Own(runtime::Bool{ res });
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args): class_inst_(class_), args_(std::move(args)) {
}

NewInstance::NewInstance(const runtime::Class& class_): class_inst_(class_) {
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    std::vector<runtime::ObjectHolder> args_values;
    for (const auto& arg : args_) {
        args_values.push_back(std::move(arg->Execute(closure, context)));
    }
    if (class_inst_.HasMethod(INIT_METHOD, args_.size())) {
        class_inst_.Call(INIT_METHOD, std::move(args_values), context);
    }
    
    return runtime::ObjectHolder::Share(class_inst_);
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body): body_(std::move(body)) {
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        body_->Execute(closure, context);
        return runtime::ObjectHolder::None();
    }  catch (runtime::ObjectHolder &result) {
        return result;
    }  catch (...) {
        throw;
    }
}

}  // namespace ast