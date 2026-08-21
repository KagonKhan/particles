#ifndef YARR_UTILS_BASES_HPP
#define YARR_UTILS_BASES_HPP

// TODO: Probably unncessary since clang screams about special member functions regardless. Singleton is only useful
///@brief disables all special members
template <typename Derived>
class PureStatic
{
public:
    PureStatic()                              = delete;
    ~PureStatic()                             = delete;
    PureStatic(const PureStatic&)             = delete;
    PureStatic(PureStatic&&)                  = delete;
    PureStatic &operator =(const PureStatic&) = delete;
    PureStatic &operator =(PureStatic&&)      = delete;
};

///@brief disables all special members but ctor
template <typename Derived>
class Immovable
{
public:
    Immovable()  = default;
    ~Immovable() = default;

    Immovable(const Immovable&)             = delete;
    Immovable(Immovable&&)                  = delete;
    Immovable &operator =(const Immovable&) = delete;
    Immovable &operator =(Immovable&&)      = delete;
};


///@brief enables getInstance function. Disables normal creation.
template <typename Derived>
class Singleton
{
public:
    static Derived& getInstance()
    {
        static Derived instance;
        return instance;
    }

protected:
    Singleton()  = default;
    ~Singleton() = default;

    Singleton(const Singleton&)             = delete;
    Singleton(Singleton&&)                  = delete;
    Singleton &operator =(const Singleton&) = delete;
    Singleton &operator =(Singleton&&)      = delete;
};

#endif // YARR_UTILS_BASES_HPP
