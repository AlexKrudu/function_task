#ifndef FUNC_LECT_FUNCTION_H
#define FUNC_LECT_FUNCTION_H

#endif //FUNC_LECT_FUNCTION_H

struct bad_function_call : std::exception {
    [[nodiscard]] char const *what() const noexcept override {
        return "empty function call";
    }
};


template<typename T>
constexpr bool is_small_obj = sizeof(T) <= sizeof(void *)
                              && (alignof(void *) % alignof(T) == 0) && std::is_nothrow_move_constructible_v<T>;

template<typename R, typename... Args>
struct storage;

template<typename R, typename... Args>
struct methods {
    typedef R (*invoke_fn_t)(storage<R, Args...> const *, Args...);

    typedef void (*deleter_fn_t)(storage<R, Args...> *);

    typedef void (*copy_fn_t)(storage<R, Args...> *, storage<R, Args...> const *);

    typedef void (*move_fn_t)(storage<R, Args...> *, storage<R, Args...> *);


    invoke_fn_t invoke;
    deleter_fn_t deleter;
    copy_fn_t copier;
    move_fn_t mover;
};

template<typename R, typename... Args>
methods<R, Args...> const *get_empty_methods() {
    static constexpr methods<R, Args...> table = {
            [](storage<R, Args...> const *, Args...) -> R {
                throw bad_function_call();
            },
            [](storage<R, Args...> *) {

            },
            [](storage<R, Args...> *dest, storage<R, Args...> const *src) {
                dest->methods_lst = src->methods_lst;
            },
            [](storage<R, Args...> *dest, storage<R, Args...> *src) {
                dest->methods_lst = std::move(src->methods_lst);
            }

    };
    return &table;
}

template<typename R, typename... Args>
struct storage {
    storage() noexcept: obj(), methods_lst(get_empty_methods<R, Args...>()) {}

    typename std::aligned_storage<sizeof(void *), alignof(void *)>::type obj;

    methods<R, Args...> const *methods_lst;
};


template<typename T, bool isSmall>
struct object_traits;

template<typename T>
struct object_traits<T, true> {
    template<typename R, typename... Args>
    static methods<R, Args...> const *get_methods() {
        static constexpr methods<R, Args...> table{
                [](storage<R, Args...> const *src, Args... args) {
                    return (reinterpret_cast<const T &>(src->obj))(std::forward<Args>(args)...);
                },
                [](storage<R, Args...> *src) {
                    delete reinterpret_cast<T *&>(src->obj);
                    src->methods_lst = get_empty_methods<R, Args...>();
                },
                [](storage<R, Args...> *dest, storage<R, Args...> const *src) {
                    new(&dest->obj) T(reinterpret_cast<const T &>(src->obj));
                    dest->methods_lst = src->methods_lst;
                },
                [](storage<R, Args...> *dest, storage<R, Args...> *src) {
                    new(&dest->obj) T(std::move(*reinterpret_cast<T const *>(src)));
                    dest->methods_lst = src->methods_lst;
                    src->methods_lst = get_empty_methods<R, Args...>();
                }
        };
        return &table;
    }

};


template<typename F>
struct function;

template<typename R, typename... Args>
struct function<R(Args...)> {
public:
    function() noexcept {
        stg.methods_lst = get_empty_methods<R, Args...>();
    };

    function(function const &other) {

    };

    function(function &&other) noexcept {

    };

    template<typename T>
    function(T val) {
        stg.methods_lst = object_traits<T, true>::template get_methods<R, Args...>();
        new(&stg.obj) T(val);
    };

    function &operator=(function const &rhs) = default;

    function &operator=(function &&rhs) noexcept = default;

    ~function() = default;

    explicit operator bool() const noexcept {
        return stg.methods_lst != get_empty_methods<R, Args...>();
    };

    R operator()(Args... args) const {
        return stg.methods_lst->invoke(&stg, std::forward<Args>(args)...); // govno kakoe to
    };

    template<typename T>
    T *target() noexcept {

    };

    template<typename T>
    T const *target() const noexcept {

    };

private:
    storage<R, Args...> stg;
};
