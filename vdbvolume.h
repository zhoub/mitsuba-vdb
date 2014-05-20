#include <mitsuba/core/properties.h>
#include <mitsuba/render/volume.h>

MTS_NAMESPACE_BEGIN

class MTS_EXPORT_RENDER VdbDataSource : public VolumeDataSource
{
public :

    VdbDataSource(const Properties &props);

    VdbDataSource(Stream *stream, InstanceManager *manager);

    virtual ~VdbDataSource();

	virtual bool supportsFloatLookups() const;

	virtual Float lookupFloat(const Point &p) const;

	virtual bool supportsVectorLookups() const;

	virtual Vector lookupVector(const Point &p) const;

    virtual void serialize(Stream *stream, InstanceManager *manager) const;

	virtual Float getStepSize() const;

	virtual Float getMaximumFloatValue() const;

    MTS_DECLARE_CLASS()

private :

    bool open();

    std::string m_filename;
    std::string m_fieldname;
} ;

MTS_IMPLEMENT_CLASS_S(VdbDataSource, false, VolumeDataSource);
MTS_EXPORT_PLUGIN(VdbDataSource, "OpenVDB data source");
MTS_NAMESPACE_END
