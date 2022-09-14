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
        if (!object) 
        {
            return false;
        }
        if (((object.TryAs<Number>() != nullptr) && (object.TryAs<Number>()->GetValue() != 0))    // если Number и не ноль
            || ((object.TryAs<Bool>() != nullptr) && (object.TryAs<Bool>()->GetValue() == true))    //  если Bool и true
            || ((object.TryAs<String>() != nullptr) && (object.TryAs<String>()->GetValue().size() != 0)))   // если  не пустая строка
        {
            return true;
        }
        return false;
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        //есть метод __str__  использум его
        if (this->HasMethod("__str__"s, 0))
        {
            this->Call("__str__"s, {}, context)->Print(os, context);
        }
        else
        {
            // выводим просто адрес объекта
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        //проверяем есть ли в vtable метод
        auto method_ptr = class_.GetMethod(method);
        if (method_ptr != nullptr)
        {
            //проверяем совпадение числа аргументов
            if (method_ptr->formal_params.size() == argument_count)
            {
                return true;
            }
        }
        return false;
    }

    Closure& ClassInstance::Fields() {
        return fields_;
    }

    const Closure& ClassInstance::Fields() const {
        return fields_;
    }

    ClassInstance::ClassInstance(const Class& cls) :class_(cls){
    }

    ObjectHolder ClassInstance::Call(const std::string& method,
        const std::vector<ObjectHolder>& actual_args,
        Context& context) {
        if (this->HasMethod(method, actual_args.size()))
        {
            // параметр self аналог указателя this в C++  
            Closure closure = { {"self", ObjectHolder::Share(*this)} };
            // Получаем указатель на метод из таблицы виртуальных функций
            auto method_ptr = class_.GetMethod(method);

            for (size_t i = 0; i < method_ptr->formal_params.size(); ++i)
            {
                std::string arg = method_ptr->formal_params[i];
                closure[arg] = actual_args[i];//имя_параметра = значение_параметра
            }
            return method_ptr->body->Execute(closure, context);
        }
        else
        {
            throw std::runtime_error("Call for a not defined method"s);
        }
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent) : 
        name_(std::move(name)), methods_(std::move(methods)), parent_(std::move(parent)) {
        if (parent_ != nullptr)
        {
            //запмсываем в vtable родительские методы
            for (const auto& parent_method : parent_->methods_)
            {
                vtable_[parent_method.name] = &parent_method;
            }
        }
        //методы с одинаковыми именами перезапишутся 
        for (const auto& method : methods_)
        {
            vtable_[method.name] = &method;
        }
    }

    const Method* Class::GetMethod(const std::string& name) const {
        if (vtable_.count(name) != 0)
        {
            return vtable_.at(name);
        }
        return nullptr;
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return name_;
    }

    void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
        os << "Class "sv << GetName();
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }


    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        // lhs и rhs это None возвращаем true.
        if (!lhs && !rhs)
        {
            return true;
        }

        {
            auto lhs_ptr = lhs.TryAs<Number>();
            auto rhs_ptr = rhs.TryAs<Number>();
            if ((lhs_ptr != nullptr) && (rhs_ptr != nullptr))
            {
                return lhs_ptr->GetValue() == rhs_ptr->GetValue();
            }
        }

        {
            auto lhs_ptr = lhs.TryAs<String>();
            auto rhs_ptr = rhs.TryAs<String>();
            if ((lhs_ptr != nullptr) && (rhs_ptr != nullptr))
            {
                return lhs_ptr->GetValue() == rhs_ptr->GetValue();
            }
        }

        {
            auto lhs_ptr = lhs.TryAs<Bool>();
            auto rhs_ptr = rhs.TryAs<Bool>();
            if ((lhs_ptr != nullptr) && (rhs_ptr != nullptr))
            {
                return lhs_ptr->GetValue() == rhs_ptr->GetValue();
            }
        }

        //  У lhs есть метод __eq__
        {
            auto lhs_ptr = lhs.TryAs<ClassInstance>();
            if (lhs_ptr != nullptr)
            {
                if (lhs_ptr->HasMethod("__eq__"s, 1))
                {
                    ObjectHolder result = lhs_ptr->Call("__eq__"s, { rhs }, context);
                    return result.TryAs<Bool>()->GetValue();
                }
            }
        }
        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        {
            auto lhs_ptr = lhs.TryAs<Number>();
            auto rhs_ptr = rhs.TryAs<Number>();
            if ((lhs_ptr != nullptr) && (rhs_ptr != nullptr))
            {
                return lhs_ptr->GetValue() < rhs_ptr->GetValue();
            }
        }

        {
            auto lhs_ptr = lhs.TryAs<String>();
            auto rhs_ptr = rhs.TryAs<String>();
            if ((lhs_ptr != nullptr) && (rhs_ptr != nullptr))
            {
                return lhs_ptr->GetValue() < rhs_ptr->GetValue();
            }
        }

        {
            auto lhs_ptr = lhs.TryAs<Bool>();
            auto rhs_ptr = rhs.TryAs<Bool>();
            if ((lhs_ptr != nullptr) && (rhs_ptr != nullptr))
            {
                return lhs_ptr->GetValue() < rhs_ptr->GetValue();
            }
        }

        // У lhs есть метод __lt__
        {
            auto lhs_ptr = lhs.TryAs<ClassInstance>();
            if (lhs_ptr != nullptr)
            {
                if (lhs_ptr->HasMethod("__lt__"s, 1))
                {
                    ObjectHolder result = lhs_ptr->Call("__lt__"s, { rhs }, context);
                    return result.TryAs<Bool>()->GetValue();
                }
            }
        }
        throw std::runtime_error("Cannot compare objects for less"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !(Less(lhs, rhs, context) || Equal(lhs, rhs, context));
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime