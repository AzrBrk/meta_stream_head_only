
//demostration of how you can implement complex meta algo with meta stream
#if(!defined(WEB_COMPILER))
#include"meta_stream.hpp"
#else
#include"https://raw.githubusercontent.com/AzrBrk/meta_stream_head_only/refs/heads/master/meta_stream.hpp"
#endif
#include<cstring>
#include<iostream>

using namespace meta_ios;
using namespace exp_utilities;


template<class ...Arg>
void store(auto* base_ptr, int& construct_cnt, Arg const& ...args) {
    using data_type_list = exp_list<std::remove_cvref_t<Arg>...>;
    //meta_for<ostream_t To, istream_t Source>::for_each/for_each_forward
    meta_for<
        meta_aligned_iterator,
        meta_istream<data_type_list>
    >::for_each_forward(
        protocol_call([&base_ptr, &construct_cnt](auto ptr, auto const& val) {
            ptr.emplace(base_ptr, val);
            ++construct_cnt;
            }), args...
    );
}
//customized ostream
//transformation meta_function:
struct r_op_f {
    //transformation:
    template<class this_list, class from_ins>
    using apply = typename this_list::template push_front<from_ins>;
    //initializer:
    template<class empty, class from_ins>
    using initialize = typename exp_list<>::template push_front<from_ins>;
};
//into ostream_t
using r_ostream = meta_object_init<r_op_f>;

template<class ...Args>
void destroy(std::byte* base_ptr) {
    using data_type_list = exp_list<std::remove_cvref_t<Args>...>;
    //destroy in reverse order to avoid destroying a type before its dependent types
    //with meta_stream::pipe
    using reverse_type_aligned_list = pipe::transfer<meta_istream<data_type_list>>
        ::template all_to<meta_aligned_iterator>
        ::template all_to<r_ostream>
        ::transfer::to_t;
    meta_for<
        meta_iterator,
        meta_istream<reverse_type_aligned_list>
    >::for_each(
        protocol_call([&base_ptr](auto ptr) {
            ptr.destroy(base_ptr);
            })
    );
}
template<class ...Args>
void destroy_n(std::byte* base_ptr, int N) {
    using data_type_list = exp_list<std::remove_cvref_t<Args>...>;
    using reverse_type_aligned_list = pipe::transfer<meta_istream<data_type_list>>
        ::template all_to<meta_aligned_iterator>
        ::template all_to<r_ostream>
        ::transfer::to_t;
    meta_for<
        meta_iterator,
        meta_istream<reverse_type_aligned_list>
    >::for_each(
        [&base_ptr, N](auto ptr_stream) {
            if (ptr_stream.left() < static_cast<std::size_t>(N)) {
                std::cout << "destroy_n: " << typeid(typename decltype(ptr_stream.object())::type).name() << std::endl;
                ptr_stream.object().destroy(base_ptr);

            }
        }
    );
}


template<class ...Typs>
struct dynamic_accessible_aligned_field;

template<class ...Typs>
struct dynamic_accessible_field_storage {
private:
    using member_type_istream = meta_istream_list<std::remove_cvref_t<Typs>...>;
    std::byte* ptr{ nullptr };
    std::size_t t_index{};
    template<class T>
    bool type_safe_check(std::size_t i)const {
        if constexpr (!exp_try_find<T, exp_list<Typs...>>::value) {
            return false;
        }
        bool res{ false };
        meta_for<
            meta_iterator,
            member_type_istream
        >::for_each([&res, i](auto type_stream) {
            if (i + type_stream.left() == sizeof...(Typs) - 1) {
                res = (type_stream.target_type() == typeid(std::remove_cvref_t<T>));
            }
            });
        return res;
    }
    template<class T>
    const T& c_type_safe_cast()const {
        if (!type_safe_check<T>(t_index)) {
            throw std::runtime_error("Type mismatch in dynamic_accessible_field_storage::c_type_safe_cast");
        }
        return reinterpret_cast<const T&>(*ptr);
    }
    template<class T>
    T& type_safe_cast() {
        if (!(type_safe_check<T>(t_index))) {
            throw std::runtime_error("Type mismatch in dynamic_accessible_field_storage::type_safe_cast");
        }
        return reinterpret_cast<T&>(*ptr);
    }
public:
    friend struct dynamic_accessible_aligned_field<Typs...>;

    dynamic_accessible_field_storage(std::byte* addr, std::size_t i) :ptr(addr), t_index(i) {}
    operator bool()const {
        return ptr != nullptr;
    }
    template<class T>
    void emplace(T const& val) {
        if (!type_safe_check<T>(t_index)) {
            throw std::runtime_error("Type mismatch in dynamic_accessible_field_storage::emplace");
        }
        if (!(*this))
        {
            throw std::runtime_error("Error: dynamic_accessible_field_storage::emplace::can not construct object at null pointer");
        }
        new (ptr) T{ val };
    }
    template<class T>
    void emplace(T&& val) {
        if (!type_safe_check<T>(t_index)) {
            throw std::runtime_error("Type mismatch in dynamic_accessible_field_storage::emplace");
        }
        if (!(*this))
        {
            throw std::runtime_error("Error: dynamic_accessible_field_storage::emplace::can not construct object at null pointer");
        }
        new (ptr) T{ std::move(val) };
    }
    void destroy() {
        if (*this)
        {
            meta_for<
                meta_iterator,
                meta_istream_list<Typs...>
            >::for_each([this]<class TS>(TS type_stream) {
                if (this->t_index + type_stream.left() == sizeof...(Typs) - 1) {
                    std::destroy_at(reinterpret_cast<typename TS::to::type*>(ptr));
                }
            });
        }
    }
    template<class T>
    operator const T& ()const {
        return c_cast<T>();
    }
    template<class T>
    T& cast() {
        return type_safe_cast<T>();
    }
    template<class T>
    const T& c_cast()const {
        return c_type_safe_cast<T>();
    }
    template<class T>
    dynamic_accessible_field_storage& operator=(T const& val) {
        type_safe_cast<T>() = val;
        return *this;
    }
    template<class F>
    void c_transform(F&& f)const {
        meta_for<meta_iterator,
            member_type_istream
            //calling for_each/for_each_forward without protocol_call with deliver 
            //the entire stream object to Callable, here's for stream status sense
            //so you don't need extra index caught in callable to calculate the pos
        >::for_each([&f, this](auto t_s) {
            if (t_index + t_s.left() == sizeof...(Typs) - 1) {
                std::invoke(f, this->c_cast<decltype(t_s.object())>());
            }
            }
        );
    }
    template<class F>
    void transform(F&& f) {
        meta_for<meta_iterator,
            member_type_istream
        >::for_each([&f, this](auto t_s) {
            if (t_index + t_s.left() == sizeof...(Typs) - 1) {
                std::invoke(f, this->cast<decltype(t_s.object())>());
            }
            }
        );
    }
};

template<typename ...Typs>
dynamic_accessible_field_storage<Typs...> get_my_any(std::size_t I, std::byte* base_ptr) {
    if (I >= sizeof...(Typs)) throw std::runtime_error("Error: index of out range!");
    constexpr std::size_t t_counts = sizeof...(Typs);
    using type_istream = meta_istream_list<std::remove_cvref_t<Typs>...>;
    std::byte* offset_ptr{ nullptr };
    meta_for<
        meta_aligned_iterator,
        type_istream
    >::for_each([&offset_ptr, base_ptr, I](auto ptr_stream) {
        if (I + ptr_stream.left() == t_counts - 1) {
            offset_ptr = ptr_stream.object().address_of(base_ptr);
        }
        });
    return dynamic_accessible_field_storage<Typs...>(offset_ptr, I);
}

template<class ...Typs>
class aligned_offset_iterator {
private:
    std::size_t index;
    std::byte* base_ptr;
public:
    aligned_offset_iterator(std::size_t i, std::byte* ptr) :index(i), base_ptr(ptr) {}
    dynamic_accessible_field_storage<Typs...> operator*() {
        return get_my_any<Typs...>(index, base_ptr);
    }
    aligned_offset_iterator& operator++() {
        ++index;
        return *this;
    }
    bool operator==(aligned_offset_iterator const& another)const {
        return index == another.index && base_ptr == another.base_ptr;
    }
};
template<class ...Typs>
struct dynamic_accessible_aligned_field {
    bool moved_flag{ false };
    int constructed_cnt{ 0 };
    using standard_aligned_t = double;
    static constexpr auto field_size = meta_all_transfer<
        meta_aligned_iterator, meta_istream_list<Typs..., standard_aligned_t>
    >::to_t::value;
    alignas(standard_aligned_t) std::byte field[field_size]{};
    auto operator[](std::size_t I) {
        if (I >= sizeof...(Typs)) throw std::runtime_error("Error: index of out range!");
        return *aligned_offset_iterator<Typs...>(I, field);
    }
    dynamic_accessible_aligned_field() = default;
    dynamic_accessible_aligned_field(Typs const&...args)noexcept {
        store(field, std::forward<int&>(constructed_cnt), args...);
    }

    dynamic_accessible_aligned_field(const dynamic_accessible_aligned_field& another) {
        auto copy_counstruct = protocol_call<protocols::wait_for_end, protocols::stream_to_t, to_meta_array_t>([&another, this]<std::size_t ...I>(meta_array<I...>) {
            store(this->field, std::forward<int&>(this->constructed_cnt), another.template read<I>()...);
        });
        meta_for<
            meta_ostream<exp_list<>>,
            meta_count_istream<0, sizeof...(Typs)>
        >::for_each(
            copy_counstruct
        );
    }
    dynamic_accessible_aligned_field& operator =(const dynamic_accessible_aligned_field& another) {
        if (this != &another) {
            if (constructed_cnt == count())
            {
                destroy<Typs...>(field);
            }
            else {
                destroy_n<Typs...>(field, constructed_cnt);
            }
            constructed_cnt = 0;
            auto copy_counstruct = protocol_call<protocols::wait_for_end, protocols::stream_to_t, to_meta_array_t>([&another, this]<std::size_t ...I>(meta_array<I...>) {
                store(this->field, std::forward<int&>(this->constructed_cnt), another.template read<I>()...);
            });
            meta_for<
                meta_ostream<exp_list<>>,
                meta_count_istream<0, sizeof...(Typs)>
            >::for_each(
                copy_counstruct
            );
        }
        return *this;
    }
    dynamic_accessible_aligned_field(dynamic_accessible_aligned_field&& v)noexcept {
        std::memcpy(field, v.field, field_size);
        std::memset(v.field, 0, field_size);
        v.moved_flag = true;
    }
    dynamic_accessible_aligned_field& operator =(dynamic_accessible_aligned_field&& v)noexcept {
        if (this != &v) {
            if (constructed_cnt == count())
            {
                destroy<Typs...>(field);
            }
            else {
                destroy_n<Typs...>(field, constructed_cnt);
            }
            std::memcpy(field, v.field, field_size);
            std::memset(v.field, 0, field_size);
            v.moved_flag = true;
        }
        return *this;
    }
    ~dynamic_accessible_aligned_field() {
        if (!moved_flag)
        {
            if (constructed_cnt == count())
            {
                destroy<Typs...>(field);
            }
            else {
                destroy_n<Typs...>(field, constructed_cnt);
            }
        }
    }
    constexpr auto count()const {
        return sizeof...(Typs);
    }
    template<std::size_t I>
    typename meta_ios::transfer<I + 1, meta_iterator, meta_istream_list<Typs...>>::to_t&
        read() {
        using seek_t = meta_ios::transfer<I + 1, meta_aligned_iterator, meta_istream_list<Typs...>>;
        return seek_t{}.object().get(field);
    }
    template<std::size_t I>
    const typename meta_ios::transfer<I + 1, meta_iterator, meta_istream_list<Typs...>>::to_t&
        read()const {
        using seek_t = meta_ios::transfer<I + 1, meta_aligned_iterator, meta_istream_list<Typs...>>;
        return seek_t{}.object().get(field);
    }
    auto begin() {
        return aligned_offset_iterator<Typs...>(0, field);
    }
    auto end() {
        return aligned_offset_iterator<Typs...>(sizeof...(Typs), field);
    }
};

template<class ...Typs>
dynamic_accessible_aligned_field(Typs const&...args) -> dynamic_accessible_aligned_field<Typs...>;

namespace daaf_helper {
    template<class T>
    struct daaf_storage_type {
        using type = T;
    };

    template<class T>
    struct daaf_storage_type<T&> {
        using type = std::reference_wrapper<T>;
    };

    template<class T>
    struct daaf_storage_type<const T&> {
        using type = std::reference_wrapper<const T>;
    };
}

template<class T>
using daaf_storage_t = typename daaf_helper::daaf_storage_type<T>::type;

template<class...Typs>
using daaf = dynamic_accessible_aligned_field<daaf_storage_t<Typs>...>;

template<class ...Typs>
std::ostream& operator<<(std::ostream& os, dynamic_accessible_field_storage<Typs...> const& e) {
    e.c_transform([&os](auto const& val) {
        os << val;
        });
    return os;
}

template<class F>
class daaf_function {
private:
    using callable_type = std::decay_t<F>;
public:
    using return_type = typename exp_function_info<callable_type>::return_type;
    using argument_types = typename exp_function_info<callable_type>::argument_types;
private:
    callable_type funct;
    typename argument_types::template to<daaf> args_stack{};
    return_type execute() {
        if (!ready()) {
            throw std::runtime_error("Error: not all arguments have been bound.");
        }
        return meta_for<
            meta_ostream<exp_list<>>,
            meta_count_istream<0, argument_types::length>
        >::for_each(
            protocol_call<return_type, protocols::wait_for_end, protocols::stream_to_t, to_meta_array_t>(
                [this]<std::size_t ...I>(meta_array<I...>) {
            return std::invoke(funct, this->args_stack.template read<I>()...);
        }
            )
        );
    }
    std::size_t bind_index{ 0 };
public:
    template<class Fn>
    explicit daaf_function(Fn&& f) : funct(std::forward<Fn>(f)) {}
    bool ready()const {
        return bind_index == argument_types::length;
    }
    template<class T>
    std::size_t bind(T const& val) {
        if (bind_index >= argument_types::length) {
            throw std::runtime_error("Error: all arguments have been bound.");
        }
        args_stack[bind_index].emplace(val);
        ++bind_index;
        ++args_stack.constructed_cnt;
        return bind_index;
    }
    template<class T>
    std::size_t bind(T && val) {
        if (bind_index >= argument_types::length) {
            throw std::runtime_error("Error: all arguments have been bound.");
        }
        args_stack[bind_index].emplace(std::move(val));
        ++bind_index;
        ++args_stack.constructed_cnt;
        return bind_index;
    }

    auto operator[](std::size_t I) {
        return args_stack[I];
    }
    template<class ...Args>
    return_type operator()(Args const& ...args) {
        (bind(args), ...);
        return execute();
    }
};
template<class F>
daaf_function(F&&) -> daaf_function<std::decay_t<F>>;

std::string add_1_char(std::string& str, char c, char d) {
    str += c;
    return str += d;
}

//make tuple without any component of std::tuple
template<class ...TPS>
auto my_tuple_cat(TPS && ...tps) {
    using jos = meta_jostream<exp_list<>>;
    using jis = meta_istream_list<std::remove_cvref_t<TPS>...>;

    using final_tuple_t = typename meta_all_transfer <
        jos,
        jis
    >::to_t;

    daaf_function final_make_tuple{ [] <class ...Typs>(exp_list<Typs...> guide) {
        return[](Typs const&... args)->typename final_tuple_t::template to<std::tuple>{
            return std::tuple<Typs...>{ args... };
        };
    }(final_tuple_t{}) };

    auto bind_tuple_f = [&final_make_tuple]<class TP>(auto stream, TP && tp) {
        meta_for<
            meta_ostream<exp_list<>>,
            meta_count_istream<0, exp_size<TP>>
        >::for_each(
            protocol_call<protocols::wait_for_end, protocols::stream_to_t, to_meta_array_t>([&final_make_tuple, &tp]<std::size_t ...I>(meta_array<I...>) {
            (final_make_tuple.bind(std::cref(std::get<I>(tp))), ...);
            })
        );
    };
    meta_for<
        meta_iterator,
        meta_count_istream<0, sizeof...(TPS)>
    >::for_each_forward(
        bind_tuple_f, std::forward<TPS>(tps)...
    );

    return final_make_tuple();

}

int main() {

    daaf_function func{ add_1_char };
    std::string str = "Hello";
    std::string str2 = "World";

    func.bind(std::ref(str));

    std::cout << func('a', 'b') << std::endl; // Output: Helloab

    func[0] = std::ref(str2); // Change the first argument to str2

    std::cout << func() << std::endl;// Output: Worldab

    auto tp = my_tuple_cat(std::make_tuple(1, 2.0), std::make_tuple('a', 'b'), std::make_tuple(std::string("Hello"), std::string("World")));

    meta_for <
        meta_iterator,
        meta_count_istream<0, std::tuple_size_v<decltype(tp)>>
    >::for_each([&tp](auto stream) {
        std::cout << std::get<stream.value()>(tp);
        if constexpr (stream.left()) std::cout << ',';
        });


}
