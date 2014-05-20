#include "vdbvolume.h"

#include <openvdb/openvdb.h>
#include <openvdb/math/BBox.h>
#include <openvdb/math/Ray.h>
#include <openvdb/tools/Interpolation.h>

struct VdbGridSet
{
    std::map<std::string, openvdb::GridBase::Ptr > m_grids;
};

struct VdbGridPool
{
    std::map<std::string, VdbGridSet > m_gridSets;
};

static VdbGridPool s_vdbGridPool ;

MTS_NAMESPACE_BEGIN

VdbDataSource::VdbDataSource(const Properties &props)
    :
    VolumeDataSource(props) {
    openvdb::initialize();

    //
    m_filename = props.getString("filename");
    m_fieldname = props.getString("fieldname");

    std::map<std::string, VdbGridSet>::iterator gridSetItr = s_vdbGridPool.m_gridSets.find(m_filename);
    if (gridSetItr == s_vdbGridPool.m_gridSets.end()) {
        if (open()) {
            gridSetItr = s_vdbGridPool.m_gridSets.find(m_filename);

            std::map<std::string, openvdb::GridBase::Ptr >::iterator gridItr = gridSetItr->second.m_grids.find(m_fieldname);
            if (gridItr == gridSetItr->second.m_grids.end()) {
                Log(EError, "Can't get the specific field [%s] from [%s] to read.", m_fieldname.c_str(), m_filename.c_str());
            }
        }
        else {
            Log(EError, "Can't open the file [%s].", m_filename.c_str());
        }
    }
    else if (gridSetItr->second.m_grids.find(m_fieldname) == gridSetItr->second.m_grids.end()) {
        Log(EError, "Opened the file [%s] but can't get the field [%s].", m_filename.c_str(), m_fieldname.c_str());
    }
}

VdbDataSource::VdbDataSource(Stream *stream, InstanceManager *manager)
    :
    VolumeDataSource(stream, manager) {
    m_filename = stream->readString();
    m_fieldname = stream->readString();
}

VdbDataSource::~VdbDataSource() {
}

bool VdbDataSource::supportsFloatLookups() const {
    return true;
}

Float VdbDataSource::lookupFloat(const Point &p) const {
    Float data = 0.0f;

    std::map<std::string, VdbGridSet >::const_iterator gridSetItr = s_vdbGridPool.m_gridSets.find(m_filename);
    if (gridSetItr != s_vdbGridPool.m_gridSets.end()) {
        std::map<std::string, openvdb::GridBase::Ptr >::const_iterator gridItr = gridSetItr->second.m_grids.find(m_fieldname);
        if (gridItr != gridSetItr->second.m_grids.end()) {
            const openvdb::FloatGrid::Ptr &floatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(gridItr->second);
            openvdb::tools::GridSampler<openvdb::FloatTree, openvdb::tools::BoxSampler> interpolator(floatGrid->constTree() , floatGrid->transform());
            data = interpolator.wsSample(openvdb::math::Vec3d(p.x, p.y, p.z));
        }
    }
    
    return data;
}

bool VdbDataSource::supportsVectorLookups() const {
    return true;
}

Vector VdbDataSource::lookupVector(const Point &p) const {
    return Vector();
}

void VdbDataSource::serialize(Stream *stream, InstanceManager *manager) const {
    VolumeDataSource::serialize(stream, manager);

    stream->writeString(m_filename);
    stream->writeString(m_fieldname);
}

Float VdbDataSource::getStepSize() const {
    Float stepSize = 1.0f;
    std::map<std::string, VdbGridSet >::const_iterator gridSetItr = s_vdbGridPool.m_gridSets.find(m_filename);
    if (gridSetItr != s_vdbGridPool.m_gridSets.end()) {
        std::map<std::string, openvdb::GridBase::Ptr >::const_iterator gridItr = gridSetItr->second.m_grids.find(m_fieldname);
        if (gridItr != gridSetItr->second.m_grids.end()) {
            const openvdb::math::Vec3d &voxelSize = gridItr->second->constTransform().voxelSize();
            stepSize = std::min(std::min(voxelSize.x(), voxelSize.y()) , voxelSize.z()) * 0.5;
        }
    }
    return stepSize;
}

Float VdbDataSource::getMaximumFloatValue() const {
    Float valMax = - 1.0f;

    std::map<std::string, VdbGridSet >::const_iterator gridSetItr = s_vdbGridPool.m_gridSets.find(m_filename);
    if (gridSetItr != s_vdbGridPool.m_gridSets.end()) {
        std::map<std::string, openvdb::GridBase::Ptr >::const_iterator gridItr = gridSetItr->second.m_grids.find(m_fieldname);
        if (gridItr != gridSetItr->second.m_grids.end()) {
            const openvdb::FloatGrid::Ptr &floatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(gridItr->second);
            Float valMin = 0.0f;
            floatGrid->evalMinMax(valMin, valMax);
        }
    }

    return valMax;
}

bool VdbDataSource::open() {
    bool success = false ;

    try {
        boost::scoped_ptr< openvdb::io::File > file(new openvdb::io::File(m_filename));
        if (file->open()) {
            s_vdbGridPool.m_gridSets.insert(std::make_pair(m_filename, VdbGridSet()));
            std::map<std::string, VdbGridSet>::iterator gridSetItr = s_vdbGridPool.m_gridSets.find(m_filename);

            //
            openvdb::math::Vec3d bbMin = openvdb::math::Vec3d(  std::numeric_limits< double >::max());
            openvdb::math::Vec3d bbMax = openvdb::math::Vec3d(- std::numeric_limits< double >::max());
            openvdb::GridPtrVecPtr grids = file->readAllGridMetadata() ;
            for (openvdb::GridPtrVec::const_iterator itr = grids->begin(); itr != grids->end(); ++ itr) {
                const openvdb::Vec3i & bbMinI = (*itr)->metaValue<openvdb::Vec3i>(openvdb::GridBase::META_FILE_BBOX_MIN);
                const openvdb::Vec3i & bbMaxI = (*itr)->metaValue<openvdb::Vec3i>(openvdb::GridBase::META_FILE_BBOX_MAX);
                bbMin = openvdb::math::minComponent(bbMin , (*itr)->indexToWorld(bbMinI));
                bbMax = openvdb::math::maxComponent(bbMax , (*itr)->indexToWorld(bbMaxI));
                const std::string &fieldname = (*itr)->getName();
                gridSetItr->second.m_grids.insert(std::make_pair(fieldname, file->readGrid((*itr)->getName())));
            }

            m_aabb.min = Point(bbMin.x(), bbMin.y(), bbMin.z());
            m_aabb.max = Point(bbMax.x(), bbMax.y(), bbMax.z());

            success = true;
        }
    }
    catch(const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    catch(...) {
    }

    return success ;
}

MTS_NAMESPACE_END
