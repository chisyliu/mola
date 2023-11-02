/* -------------------------------------------------------------------------
 *   A Modular Optimization framework for Localization and mApping  (MOLA)
 *
 * Copyright (C) 2018-2023 Jose Luis Blanco, University of Almeria
 * Licensed under the GNU GPL v3 for non-commercial applications.
 *
 * This file is part of MOLA.
 * MOLA is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * MOLA is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MOLA. If not, see <https://www.gnu.org/licenses/>.
 * ------------------------------------------------------------------------- */
/**
 * @file   DualVoxelPointCloud.h
 * @brief  Point cloud stored as a dual-resolution voxel map
 * @author Jose Luis Blanco Claraco
 * @date   Oct 31, 2023
 */
#pragma once

#include <mola_metric_maps/index3d_t.h>
#include <mrpt/containers/vector_with_small_size_optimization.h>
#include <mrpt/core/round.h>
#include <mrpt/img/TColor.h>
#include <mrpt/img/color_maps.h>
#include <mrpt/maps/CMetricMap.h>
#include <mrpt/math/TBoundingBox.h>
#include <mrpt/math/TPoint3D.h>

#include <functional>
#include <optional>

namespace mola
{
/** DualVoxelPointCloud: a pointcloud stored in two dual hash'ed voxel maps,
 *  one for decimation purposes only, and another for nearest-neighbor search.
 *
 */
class DualVoxelPointCloud : public mrpt::maps::CMetricMap
{
    DEFINE_SERIALIZABLE(DualVoxelPointCloud, mola)
   public:
    /** @name Basic API for construction and main parameters
     *  @{ */

    /**
     * @brief Constructor / default ctor
     * @param decimation_size Voxel size [meters] for decimation purposes.
     * @param max_nn_radius Maximum radius [meters] for nearest-neighbor search.
     * @param max_points_per_voxel If !=0, defines a maximum number of points
     * per voxel.
     */
    DualVoxelPointCloud(
        float decimation_size = 0.20f, float max_nn_radius = 0.60f,
        uint32_t max_points_per_voxel = 0);

    ~DualVoxelPointCloud();

    /** Reset the main voxel parameters, and *clears* all current map contents
     */
    void setVoxelProperties(
        float decimation_size, float max_nn_radius,
        uint32_t max_points_per_voxel = 0);

    /** @} */

    /** @name Data structures and compile-time parameters
     *  @{ */

    /// Size of the std::array for the small-size optimization container:
    constexpr static std::size_t SSO_LENGTH = 16;

    /// shortcut to save typing:
    template <typename T, std::size_t LEN>
    using vector_sso =
        mrpt::containers::vector_with_small_size_optimization<T, LEN>;

    struct VoxelData
    {
       public:
        VoxelData() = default;

        const auto& points() const { return points_; }

        void insertPoint(const mrpt::math::TPoint3Df& p);

        /** Gets the mean of all points in the voxel. Throws if empty. */
        const mrpt::math::TPoint3Df& mean() const;

        const auto& neighbors() const { return neighbors_; }
        auto&       neighbors() { return neighbors_; }

       private:
        vector_sso<mrpt::math::TPoint3Df, SSO_LENGTH> points_;
        mutable std::optional<mrpt::math::TPoint3Df>  mean_;

        // We can store pointers safely, since the unordered_map container
        // does not invalidate them.
        std::unordered_map<
            index3d_t, std::optional<std::reference_wrapper<const VoxelData>>,
            index3d_hash>
            neighbors_;
    };

    using voxel_map_t = std::unordered_map<index3d_t, VoxelData, index3d_hash>;

    /** @} */

    /** @name Data access API
     *  @{ */
    // clear(): available in base class

    /** Insert one point into the dual voxel map */
    void insertPoint(const mrpt::math::TPoint3Df& pt);

    /** Query for the closest neighbor of a given point.
     *  \return true if nearest neighbor was found.
     */
    bool nn_find_nearest(
        const mrpt::math::TPoint3Df& queryPoint,
        mrpt::math::TPoint3Df& outNearest, float& outDistanceSquared) const;

    const voxel_map_t& voxels() const { return voxels_; }

    /** Computes the bounding box of all the points, or (0,0 ,0,0, 0,0) if
     * there are no points. Results are cached unless the map is somehow
     * modified to avoid repeated calculations.
     */
    mrpt::math::TBoundingBoxf boundingBox() const;

    void visitAllPoints(
        const std::function<void(const mrpt::math::TPoint3Df&)>& f) const;

    void visitAllVoxels(
        const std::function<void(const index3d_t&, const VoxelData&)>& f) const;

    /** Save to a text file. Each line contains "X Y Z" point coordinates.
     *  Returns false if any error occured, true elsewere.
     */
    bool saveToTextFile(const std::string& file) const;

    /** @} */

    /** @name Public virtual methods implementation for CMetricMap
     *  @{ */

    /** Returns a short description of the map. */
    std::string asString() const override;

    void getVisualizationInto(
        mrpt::opengl::CSetOfObjects& outObj) const override;

    /** Returns true if the map is empty */
    bool isEmpty() const override;

    /** This virtual method saves the map to a file "filNamePrefix"+<
     * some_file_extension >, as an image or in any other applicable way (Notice
     * that other methods to save the map may be implemented in classes
     * implementing this virtual interface).  */
    void saveMetricMapRepresentationToFile(
        const std::string& filNamePrefix) const override;

    /** @} */

    /** Options used when evaluating "computeObservationLikelihood" in the
     * derived classes.
     * \sa CObservation::computeObservationLikelihood
     */
    struct TLikelihoodOptions : public mrpt::config::CLoadableOptions
    {
        TLikelihoodOptions() = default;

        void loadFromConfigFile(
            const mrpt::config::CConfigFileBase& source,
            const std::string& section) override;  // See base docs
        void dumpToTextStream(
            std::ostream& out) const override;  // See base docs

        void writeToStream(mrpt::serialization::CArchive& out) const;
        void readFromStream(mrpt::serialization::CArchive& in);

        /** Sigma (standard deviation, in meters) of the Gaussian observation
         *  model used to model the likelihood (default= 0.5 m) */
        double sigma_dist = 0.5;

        /** Maximum distance in meters to consider for the numerator divided by
         * "sigma_dist", so that each point has a minimum (but very small)
         * likelihood to avoid underflows (default=1.0 meters) */
        double max_corr_distance = 1.0;

        /** Speed up the likelihood computation by considering only one out of N
         * rays (default=10) */
        uint32_t decimation = 10;
    };
    TLikelihoodOptions likelihoodOptions;

    /** Rendering options, used in getAs3DObject()
     */
    struct TRenderOptions : public mrpt::config::CLoadableOptions
    {
        void loadFromConfigFile(
            const mrpt::config::CConfigFileBase& source,
            const std::string& section) override;  // See base docs
        void dumpToTextStream(
            std::ostream& out) const override;  // See base docs

        /** Binary dump to stream - used in derived classes' serialization */
        void writeToStream(mrpt::serialization::CArchive& out) const;
        /** Binary dump to stream - used in derived classes' serialization */
        void readFromStream(mrpt::serialization::CArchive& in);

        float point_size = 1.0f;

        /** If true, when rendering a voxel map only the mean point per voxel
         * will be rendered instead of all contained points. */
        bool show_mean_only = true;

        /** Color of points. Superseded by colormap if the latter is set. */
        mrpt::img::TColorf color{.0f, .0f, 1.0f};

        /** Colormap for points (index is "z" coordinates) */
        mrpt::img::TColormap colormap = mrpt::img::cmHOT;

        /** If colormap!=mrpt::img::cmNONE, use this coordinate
         *  as color index: 0=x  1=y  2=z
         */
        uint8_t recolorizeByCoordinateIndex = 2;
    };
    TRenderOptions renderOptions;

   public:
    // Interface for use within a mrpt::maps::CMultiMetricMap:
    MAP_DEFINITION_START(DualVoxelPointCloud)
    float  decimation_size      = 0.20f;
    float  max_nn_radius        = 0.60f;
    size_t max_points_per_voxel = 0;

    mola::DualVoxelPointCloud::TLikelihoodOptions likelihoodOpts;
    mola::DualVoxelPointCloud::TRenderOptions     renderOpts;
    MAP_DEFINITION_END(DualVoxelPointCloud)

   private:
    float    decimation_size_      = 0.20f;
    float    max_nn_radius_        = 0.60f;
    uint32_t max_points_per_voxel_ = 0;

    // Calculated from the above, in setVoxelProperties()
    float   decimation_size_inv_ = 1.0f / decimation_size_;
    float   max_nn_radius_sqr_   = max_nn_radius_ * max_nn_radius_;
    int32_t nn_to_decim_ratio_   = 3;  // ceiling of nn_radius / decim_size

    /** Decimation voxel map */
    voxel_map_t voxels_;

    struct CachedData
    {
        CachedData() = default;

        void reset() { *this = CachedData(); }

        mutable std::optional<mrpt::math::TBoundingBoxf> boundingBox_;
    };

    CachedData cached_;

    int32_t coord2idx(float xyz) const
    {
        return mrpt::round(xyz * decimation_size_inv_);
    }

    /// returns the coordinate of the voxel center
    float idx2coord(int32_t idx) const { return idx * decimation_size_; }

    void internalUpdateNNs(const index3d_t& voxelIdxs, const VoxelData& voxel);

   protected:
    // See docs in base CMetricMap class:
    void internal_clear() override;

   private:
    // See docs in base CMetricMap class:
    bool internal_insertObservation(
        const mrpt::obs::CObservation&                   obs,
        const std::optional<const mrpt::poses::CPose3D>& robotPose =
            std::nullopt) override;
    // See docs in base class
    double internal_computeObservationLikelihood(
        const mrpt::obs::CObservation& obs,
        const mrpt::poses::CPose3D&    takenFrom) const override;

    double internal_computeObservationLikelihoodPointCloud3D(
        const mrpt::poses::CPose3D& pc_in_map, const float* xs, const float* ys,
        const float* zs, const std::size_t num_pts) const;

    /** - (xs,ys,zs): Sensed point local coordinates in the robot frame.
     *  - pc_in_map: SE(3) pose of the robot in the map frame.
     */
    void internal_insertPointCloud3D(
        const mrpt::poses::CPose3D& pc_in_map, const float* xs, const float* ys,
        const float* zs, const std::size_t num_pts);

    // See docs in base class
    bool internal_canComputeObservationLikelihood(
        const mrpt::obs::CObservation& obs) const override;
};

}  // namespace mola
