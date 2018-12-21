/* -------------------------------------------------------------------------
 *   A Modular Optimization framework for Localization and mApping  (MOLA)
 * Copyright (C) 2018-2019 Jose Luis Blanco, University of Almeria
 * See LICENSE for license information.
 * ------------------------------------------------------------------------- */
/**
 * @file   ExecutableBase.h
 * @brief  Virtual interface for objects that can be run into MOLA
 * @author Jose Luis Blanco Claraco
 * @date   Dec 14, 2018
 */
#pragma once

#include <mrpt/system/COutputLogger.h>
#include <mrpt/system/CTimeLogger.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mola
{
using Profiler      = mrpt::system::CTimeLogger;
using ProfilerEntry = mrpt::system::CTimeLoggerEntry;

/** Base virtual class for all executable (nodelets-like) units inside a SLAM
 * system.
 * \ingroup mola_kernel_grp */
class ExecutableBase : public mrpt::system::COutputLogger,
                       std::enable_shared_from_this<ExecutableBase>
{
   public:
    ExecutableBase();
    virtual ~ExecutableBase();

    using Ptr = std::shared_ptr<ExecutableBase>;

    /** Get as shared_ptr via enable_shared_from_this<> */
    Ptr getAsPtr() { return shared_from_this(); }

    /** @name Virtual interface of any ExecutableBase
     *{ */
    virtual void initialize_common(const std::string& cfg_block) = 0;
    virtual void initialize(const std::string& cfg_block);
    virtual void spinOnce() = 0;
    /** @} */

    /** @name Directory services
     *{ */

    /** A name server function to search for other ExecutableBase objects in my
     * running system. Empty during ctor, should be usable from
     * initialize_common() and initialize().
     * \note In a standard system, this is implemented by
     * MolaLauncherApp::nameServerImpl()
     * \sa findService() */
    std::function<Ptr(const std::string&)> nameServer_;

    /** Finds (an)other ExecutableBase(s) by its expected Interface, that is,
     * a virtual base class.
     * \sa nameServer_ */
    template <class Interface>
    std::vector<Ptr> findService() const;
    /** @} */

    /** Time profiler (disabled by default). All profilers can be globally
     * enabled from MolaLauncherApp. */
    Profiler profiler_{false};
};

// Impl:
template <class Interface>
std::vector<ExecutableBase::Ptr> ExecutableBase::findService() const
{
    ASSERT_(nameServer_);
    std::vector<ExecutableBase::Ptr> ret;
    for (size_t idx = 0;; ++idx)
    {
        using namespace std::string_literals;
        const auto req = "["s + std::to_string(idx);
        auto       mod = nameServer_(req);
        if (!mod) break;  // end of list of modules
        if (std::dynamic_pointer_cast<Interface>(mod))
            ret.emplace_back(std::move(mod));
    }
    return ret;
}

}  // namespace mola
