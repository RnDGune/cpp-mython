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
  

    ObjectHolder Assignment::Execute(Closure& closure,[[maybe_unused]] Context& context) {
        closure[var_] = std::move(rv_->Execute(closure, context));
        return closure.at(var_);
    }

    ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        if (dotted_ids_.size() > 0)
        {
            runtime::ObjectHolder result;
            Closure* current_closure_ptr = &closure;
            for (const auto& arg : dotted_ids_)
            {
                auto arg_it = current_closure_ptr->find(arg);
                if (arg_it == current_closure_ptr->end())
                {
                    throw std::runtime_error("Invalid argument name in VariableValue::Execute()"s);
                }
                result = arg_it->second;
                auto next_dotted_arg_ptr = result.TryAs<runtime::ClassInstance>();
                if (next_dotted_arg_ptr)
                {
                    current_closure_ptr = &next_dotted_arg_ptr->Fields();
                }
            }
            return result;
        }
        throw std::runtime_error("No arguments specified for VariableValue::Execute()"s);
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

   

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        for (size_t i = 0; i < args_.size(); ++i)
        {
            // если  не первый элемент, нужно вывести разделяющий пробел
            if (i > 0)
            {
                context.GetOutputStream() << " "s;
            }

            runtime::ObjectHolder result = args_[i]->Execute(closure, context);

            //выводим результат в поток если он не None (nullptr)
            if (result)
            {
                result->Print(context.GetOutputStream(), context);
            }
            else
            {
                context.GetOutputStream() << "None"s;
            }
        }
        //перевод строки в конце вывода
        context.GetOutputStream() << std::endl;
        return runtime::ObjectHolder::None();
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        //если указателя ytт нет, возвращаем None()
        if (!object_)
        {
            return runtime::ObjectHolder::None();
        }

        runtime::ObjectHolder callable_object = object_->Execute(closure, context);
        auto callable_object_ptr = callable_object.TryAs<runtime::ClassInstance>();
        if (callable_object_ptr != nullptr)
        {
            std::vector<runtime::ObjectHolder> args_values;
            for (const auto& arg : args_)
            {
                args_values.push_back(std::move(arg->Execute(closure, context)));
            }
            runtime::ObjectHolder result = callable_object_ptr->Call(method_, args_values, context);
            return result;
        }
        return runtime::ObjectHolder::None();
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        if (!argument_)
        {
            return runtime::ObjectHolder::Own(runtime::String{ "None"s });
        }
        runtime::ObjectHolder exec_result = argument_->Execute(closure, context);
        if (!exec_result)  
        {
            return runtime::ObjectHolder::Own(runtime::String{ "None"s });
        }
        runtime::DummyContext dummy_context;
        exec_result.Get()->Print(dummy_context.GetOutputStream(), dummy_context);
        return runtime::ObjectHolder::Own(runtime::String{ dummy_context.output.str() });
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        if ((!lhs_) || (!rhs_))
        {
            throw std::runtime_error("No argument(s) specified for Add::Execute()"s);
        }
        runtime::ObjectHolder lhs_exec_result = lhs_->Execute(closure, context);
        runtime::ObjectHolder rhs_exec_result = rhs_->Execute(closure, context);
        {
            auto lhs_value_ptr = lhs_exec_result.TryAs<runtime::Number>();
            auto rhs_value_ptr = rhs_exec_result.TryAs<runtime::Number>();

            if ((lhs_value_ptr != nullptr) && (rhs_value_ptr != nullptr))
            {
                auto lhs_value = lhs_value_ptr->GetValue();
                auto rhs_value = rhs_value_ptr->GetValue();

                return runtime::ObjectHolder::Own(runtime::Number{ lhs_value + rhs_value });
            }
        }
        {
            auto lhs_value_ptr = lhs_exec_result.TryAs<runtime::String>();
            auto rhs_value_ptr = rhs_exec_result.TryAs<runtime::String>();
            if ((lhs_value_ptr != nullptr) && (rhs_value_ptr != nullptr))
            {
                auto& lhs_value = lhs_value_ptr->GetValue();
                auto& rhs_value = rhs_value_ptr->GetValue();
                return runtime::ObjectHolder::Own(runtime::String{ lhs_value + rhs_value });
            }
        }
        {
            auto lhs_value_ptr = lhs_exec_result.TryAs<runtime::ClassInstance>();
            if (lhs_value_ptr != nullptr)
            {
                const int ARGUMENT_NUM = 1;
                if (lhs_value_ptr->HasMethod(ADD_METHOD, ARGUMENT_NUM))
                {
                    return lhs_value_ptr->Call(ADD_METHOD, { rhs_exec_result }, context);
                }
            }
        }
        throw std::runtime_error("Incompatible argument(s) type(s) for Add::Execute()"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        if ((!lhs_) || (!rhs_))
        {
            throw std::runtime_error("No argument(s) specified for Sub::Execute()"s);
        }
        runtime::ObjectHolder lhs_exec_result = lhs_->Execute(closure, context);
        runtime::ObjectHolder rhs_exec_result = rhs_->Execute(closure, context);
        {
            auto lhs_value_ptr = lhs_exec_result.TryAs<runtime::Number>();
            auto rhs_value_ptr = rhs_exec_result.TryAs<runtime::Number>();

            if ((lhs_value_ptr != nullptr) && (rhs_value_ptr != nullptr))
            {
                auto lhs_value = lhs_value_ptr->GetValue();
                auto rhs_value = rhs_value_ptr->GetValue();

                return runtime::ObjectHolder::Own(runtime::Number{ lhs_value - rhs_value });
            }

        }

        throw std::runtime_error("Incompatible argument(s) type(s) for Sub::Execute()"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        if ((!lhs_) || (!rhs_))
        {
            throw std::runtime_error("No argument(s) specified for Mult::Execute()"s);
        }
        runtime::ObjectHolder lhs_exec_result = lhs_->Execute(closure, context);
        runtime::ObjectHolder rhs_exec_result = rhs_->Execute(closure, context);
        {
            auto lhs_value_ptr = lhs_exec_result.TryAs<runtime::Number>();
            auto rhs_value_ptr = rhs_exec_result.TryAs<runtime::Number>();

            if ((lhs_value_ptr != nullptr) && (rhs_value_ptr != nullptr))
            {
                auto lhs_value = lhs_value_ptr->GetValue();
                auto rhs_value = rhs_value_ptr->GetValue();

                return runtime::ObjectHolder::Own(runtime::Number{ lhs_value * rhs_value });
            }

        }
        throw std::runtime_error("Incompatible argument(s) type(s) for Mult::Execute()"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        if ((!lhs_) || (!rhs_))
        {
            throw std::runtime_error("No argument(s) specified for Div::Execute()"s);
        }
        runtime::ObjectHolder lhs_exec_result = lhs_->Execute(closure, context);
        runtime::ObjectHolder rhs_exec_result = rhs_->Execute(closure, context);

        {
            auto lhs_value_ptr = lhs_exec_result.TryAs<runtime::Number>();
            auto rhs_value_ptr = rhs_exec_result.TryAs<runtime::Number>();
            if ((lhs_value_ptr != nullptr) && (rhs_value_ptr != nullptr))
            {
                auto lhs_value = lhs_value_ptr->GetValue();
                auto rhs_value = rhs_value_ptr->GetValue();
                if (rhs_value == 0)
                {
                    throw std::runtime_error("Division by zero in Div::Execute()"s);
                }
                return runtime::ObjectHolder::Own(runtime::Number{ lhs_value / rhs_value });
            }

        }
        throw std::runtime_error("Incompatible argument(s) type(s) for Div::Execute()"s);
    }

    void Compound::AddStatement(std::unique_ptr<Statement> stmt)
    {
        statements_.push_back(std::move(stmt));
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (const auto& statement : statements_)
        {
            statement->Execute(closure, context);
        }
        return runtime::ObjectHolder::None();
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        if (!statement_)
        {
            throw ReturnException(runtime::ObjectHolder::None());
        }
        runtime::ObjectHolder result = statement_->Execute(closure, context);
        throw ReturnException(result);
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure,[[maybe_unused]] Context& context) {
        auto class_ptr = cls_.TryAs<runtime::Class>();
        closure[class_ptr->GetName()] = std::move(cls_);
        return runtime::ObjectHolder::None();
    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        runtime::ObjectHolder object_value = object_.Execute(closure, context);
        if (!object_value)
        {
            return runtime::ObjectHolder::None();
        }
        auto object_value_ptr = object_value.TryAs<runtime::ClassInstance>();
        object_value_ptr->Fields()[field_name_] = std::move(rv_->Execute(closure, context));
        return object_value_ptr->Fields().at(field_name_);
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (!condition_)
        {
            throw std::runtime_error("No condition specified for IfElse::Execute()"s);
        }
        runtime::ObjectHolder condition_result = condition_->Execute(closure, context);
        if (runtime::IsTrue(condition_result))
        {
            return if_body_->Execute(closure, context);
        }
        else if (else_body_)
        {
            return else_body_->Execute(closure, context);
        }

        return runtime::ObjectHolder::None();
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        if ((!lhs_) || (!rhs_))
        {
            throw std::runtime_error("Null operands specified for Or::Execute()"s);
        }
        runtime::ObjectHolder lhs_exec_result = lhs_->Execute(closure, context);
        if (runtime::IsTrue(lhs_exec_result))
        {
            return runtime::ObjectHolder::Own(runtime::Bool{ true });
        }
        runtime::ObjectHolder rhs_exec_result = rhs_->Execute(closure, context);
        if (runtime::IsTrue(rhs_exec_result))
        {
            return runtime::ObjectHolder::Own(runtime::Bool{ true });
        }
        return runtime::ObjectHolder::Own(runtime::Bool{ false });
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        if ((!lhs_) || (!rhs_))
        {
            throw std::runtime_error("Null operands specified for And::Execute()"s);
        }
        runtime::ObjectHolder lhs_exec_result = lhs_->Execute(closure, context);
        runtime::ObjectHolder rhs_exec_result = rhs_->Execute(closure, context);
        if (runtime::IsTrue(lhs_exec_result) && runtime::IsTrue(rhs_exec_result))
        {
            return runtime::ObjectHolder::Own(runtime::Bool{ true });
        }
        return runtime::ObjectHolder::Own(runtime::Bool{ false });
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        if (!argument_)
        {
            throw std::runtime_error("Null operand specified for Not::Execute()"s);
        }
        runtime::ObjectHolder arg_exec_result = argument_->Execute(closure, context);
        bool result = runtime::IsTrue(arg_exec_result);
        return runtime::ObjectHolder::Own(runtime::Bool{ !result });
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        if ((!lhs_) || (!rhs_))
        {
            throw std::runtime_error("Null operands specified for Comparison::Execute()"s);
        }
        runtime::ObjectHolder lhs_exec_result = lhs_->Execute(closure, context);
        runtime::ObjectHolder rhs_exec_result = rhs_->Execute(closure, context);
        bool result = cmp_(lhs_exec_result, rhs_exec_result, context);
        return runtime::ObjectHolder::Own(runtime::Bool{ result });
    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        if (class_instance_.HasMethod(INIT_METHOD, args_.size()))
        {
            std::vector<ObjectHolder> args_values;
            for (const auto& argument : args_)
            {
                args_values.push_back(std::move(argument->Execute(closure, context)));
            }
            class_instance_.Call(INIT_METHOD, args_values, context);
        }

        return runtime::ObjectHolder::Share(class_instance_);
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        if (!body_)
        {
            return runtime::ObjectHolder::None();
        }
        try
        {
            body_->Execute(closure, context);
        }
        catch (ReturnException& rex) 
        {
            return rex.GetValue();
        }
        return runtime::ObjectHolder::None();
    }

    runtime::ObjectHolder ReturnException::GetValue()
    {
        return ret_value_;
    }

}  // namespace ast