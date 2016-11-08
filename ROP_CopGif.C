#include "ROP_CopGif.h"

#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>

#include <COP2/COP2_Node.h>

#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <CH/CH_LocalVariable.h>

#include <UT/UT_DSOVersion.h>

#include <PXL/PXL_Raster.h>
#include <TIL/TIL_Raster.h>
#include <TIL/TIL_CopResolver.h>


#define ROP_COP_GIF_NAME "copgif"
#define ROP_COP_GIF_DESCRIPTION "COP Gif"

#define ROP_COP_GIF_DEFAULT_COP_PLANE "C"


#define ROP_COP_GIF_COP_PATH "cop_path"
#define ROP_COP_GIF_OUTPUT_FILE "output_file"
#define SOP_COP_GIF_PLANE "cop_plane"
#define ROP_COP_GIF_SEPARATOR "rop_cop_gif_separator"


static PRM_Name s_name_file_output(ROP_COP_GIF_OUTPUT_FILE, "Output File");
static PRM_Name s_name_cop_path(ROP_COP_GIF_COP_PATH, "COP Path");
static PRM_Name s_name_cop_plane(SOP_COP_GIF_PLANE, "COP2 Plane");
static PRM_Name s_name_separator(ROP_COP_GIF_SEPARATOR, "");

static PRM_Default s_default_name_cop_plane(0.0f, ROP_COP_GIF_DEFAULT_COP_PLANE);

static PRM_SpareData s_spare_file_output(PRM_SpareArgs()
    << PRM_SpareToken(PRM_SpareData::getFileChooserModeToken(), PRM_SpareData::getFileChooserModeValRead())
    << PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.gif"));


PRM_Template*
ROP_CopGif::ms_template = nullptr;


OP_TemplatePair*
ROP_CopGif::ms_template_pair = nullptr;


OP_VariablePair*
ROP_CopGif::ms_variable_pair = nullptr;


static
PRM_Template*
getTemplates()
{
    if(ROP_CopGif::ms_template)
    {
        return ROP_CopGif::ms_template;
    }

    const int template_count = 5;
    ROP_CopGif::ms_template = new PRM_Template[template_count];
    int template_idx = 0;

    ROP_CopGif::ms_template[template_idx++] = PRM_Template(PRM_SEPARATOR, 0, &s_name_separator);

    ROP_CopGif::ms_template[template_idx++] =
        PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &s_name_cop_path, 0, 0, 0, 0, &PRM_SpareData::cop2Path),

    ROP_CopGif::ms_template[template_idx++] =
        PRM_Template(PRM_FILE, 1, &s_name_file_output, 0, 0, 0, 0, &s_spare_file_output);

    ROP_CopGif::ms_template[template_idx] =
        PRM_Template(PRM_STRING, 1, &s_name_cop_plane, &s_default_name_cop_plane),

    ROP_CopGif::ms_template[template_idx++] = PRM_Template();

    return ROP_CopGif::ms_template;
}


OP_TemplatePair*
ROP_CopGif::getTemplatePair()
{
    if(!ROP_CopGif::ms_template_pair)
    {
        OP_TemplatePair* base = new OP_TemplatePair(getTemplates());
        ROP_CopGif::ms_template_pair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
    }

    return ROP_CopGif::ms_template_pair;
}


OP_VariablePair*
ROP_CopGif::getVariablePair()
{
    if(!ROP_CopGif::ms_variable_pair)
    {
        ROP_CopGif::ms_variable_pair = new OP_VariablePair(ROP_Node::myVariableList);
    }

    return ROP_CopGif::ms_variable_pair;
}


OP_Node*
ROP_CopGif::myConstructor(OP_Network* net, const char* name, OP_Operator* op)
{
    return new ROP_CopGif(net, name, op);
}


ROP_CopGif::ROP_CopGif(OP_Network* net, const char* name, OP_Operator* entry) :
    ROP_Node(net, name, entry),
    m_render_time_start(0.0f),
    m_render_time_end(0.0f)
{

}


ROP_CopGif::~ROP_CopGif()
{

}


bool
ROP_CopGif::updateParmsFlags()
{
    bool changed = ROP_Node::updateParmsFlags();
    return changed;
}


int
ROP_CopGif::startRender(int nframes, fpreal time_start, fpreal time_end)
{
    if(error() < UT_ERROR_ABORT)
    {
        executePreRenderScript(time_start);
    }

    // Record frames start and end.
    m_render_time_start = time_start;
    m_render_time_end = time_end;

    // Reset frame sizes.
    m_width = 0u;
    m_height = 0u;

    // Clear previously stored frames.
    m_frames.clear();

    return 1;
}


ROP_RENDER_CODE
ROP_CopGif::renderFrame(fpreal t, UT_Interrupt* boss)
{
    // Execute the pre-render script.
    if(!executePreFrameScript(t))
    {
        return ROP_ABORT_RENDER;
    }

    UT_String cop_image_plane;
    getImagePlaneName(t, cop_image_plane);

    UT_String cop_relative_path;
    getParamCopPath(t, cop_relative_path);

    UT_String cop_full_path;
    if(!getFullCopPath(cop_relative_path, cop_full_path))
    {
        addError(ROP_MESSAGE, "Cop Gif: Invalid COP2 path.");
        return ROP_ABORT_RENDER;
    }

    // Make sure COP node exists and we are able to look it up.
    COP2_Node* cop_node = getCOP2Node(cop_full_path, 1);
    if(!cop_node)
    {
        addError(ROP_MESSAGE, "Cop Gif: Unable to find the COP2 node.");
        return ROP_ABORT_RENDER;
    }

    // Get COP2 resolver.
    if(!m_cop_resolver)
    {
        m_cop_resolver = TIL_CopResolver::getResolver();
        if(!m_cop_resolver)
        {
            addError(ROP_MESSAGE, "Cop Gif: Unable to retrieve COP2 resolver.");
            return ROP_ABORT_RENDER;
        }
    }

    // Get COP2 raster.
    TIL_Raster* raster = m_cop_resolver->getNodeRaster(cop_full_path, cop_image_plane, "");
    if(!raster)
    {
        addError(ROP_MESSAGE, "Cop Gif: Unable to retrieve COP2 node raster.");
        return ROP_ABORT_RENDER;
    }

    // Make sure raster is valid.
    if(!raster->isValid())
    {
        addError(ROP_MESSAGE, "Cop Gif: Invalid COP2 node raster.");
        return ROP_ABORT_RENDER;
    }

    // Make sure we support raster format.
    if(!isCopDataFloat(raster->getFormat()))
    {
        addError(ROP_MESSAGE, "Cop Gif: Non floating raster formats are not supported.");
        return ROP_ABORT_RENDER;
    }

    // Process raster.
    if(!processFrameRaster(t, raster))
    {
        addError(ROP_MESSAGE, "Cop Gif: Unable to process COP2 node raster.");
        return ROP_ABORT_RENDER;
    }

    // Return raster back to resolver.
    m_cop_resolver->returnRaster(raster);

    // Execute the post-render script.
    if(error() < UT_ERROR_ABORT)
    {
        executePostFrameScript(t);
    }

    return ROP_CONTINUE_RENDER;
}


ROP_RENDER_CODE
ROP_CopGif::endRender()
{
    if(error() < UT_ERROR_ABORT)
    {
        executePostRenderScript(m_render_time_end);
    }

    // Get output path.
    UT_String output_file;
    getParamOutputFileName(0, output_file);

    if(!output_file || !output_file.isstring())
    {
        addError(ROP_MESSAGE, "Cop Gif: Invalid output gif file specified.");
        return ROP_ABORT_RENDER;
    }

    GifWriter gif_writer;
    if(!GifBegin(&gif_writer, output_file.c_str(), m_width, m_height, 0))
    {
        addError(ROP_MESSAGE, "Cop Gif: Unable to initialize gif writer.");
        return ROP_ABORT_RENDER;
    }

    for(int frame_idx = 0; frame_idx < m_frames.size(); ++frame_idx)
    {
        const UT_Array<unsigned char>& frame_data = m_frames(frame_idx);
        if(!frame_data.size())
        {
            addError(ROP_MESSAGE, "Cop Gif: Invalid raster frame data.");
            return ROP_ABORT_RENDER;
        }

        const unsigned char* frame_start = &frame_data(0);

        if(!GifWriteFrame(&gif_writer, (const uint8_t*) frame_start, m_width, m_height, 0))
        {
            addError(ROP_MESSAGE, "Cop Gif: Unable to write a gif frame.");
            return ROP_ABORT_RENDER;
        }
    }

    if(!GifEnd(&gif_writer))
    {
        addError(ROP_MESSAGE, "Cop Gif: Unable to finalize gif writer.");
        return ROP_ABORT_RENDER;
    }

    return ROP_CONTINUE_RENDER;
}


bool
ROP_CopGif::getFullCopPath(const UT_String& relative_path, UT_String& full_path) const
{
    OP_Node* node = findNode(relative_path);
    if(!node)
    {
        return false;
    }

    // If this is not a cop2 node.
    if(COP2_OPTYPE_ID != node->getOpTypeID())
    {
        return false;
    }

    node = (OP_Node*) CAST_COP2NODE(node);
    if(!node)
    {
        return false;
    }

    node->getFullPath(full_path);
    return true;
}


void
ROP_CopGif::getParamCopPath(fpreal t, UT_String& cop_path) const
{
    evalString(cop_path, ROP_COP_GIF_COP_PATH, 0, t);
}


void
ROP_CopGif::getParamOutputFileName(fpreal t, UT_String& output_file) const
{
    evalString(output_file, ROP_COP_GIF_OUTPUT_FILE, 0, t);
}


void
ROP_CopGif::getImagePlaneName(fpreal t, UT_String& image_plane_name) const
{
    evalString(image_plane_name, SOP_COP_GIF_PLANE, 0, t);
}


unsigned int
ROP_CopGif::getCopDataChannelCount(PXL_Packing packing) const
{
    switch(packing)
    {
        case PACK_SINGLE:
        {
            return 1u;
        }

        case PACK_DUAL:
        {
            return 2u;
        }

        case PACK_RGB:
        {
            return 3u;
        }

        case PACK_RGBA:
        {
            return 4u;
        }

        default:
        {
            break;
        }
    }

    return 0u;
}


bool
ROP_CopGif::isCopDataFloat(PXL_DataFormat data_format) const
{
    switch(data_format)
    {
        case PXL_FLOAT16:
        case PXL_FLOAT32:
        {
            return true;
        }

        default:
        {
            break;
        }
    }

    return false;
}


void
ROP_CopGif::getFramePixel(const unsigned char* pixel_data, PXL_Packing packing, PXL_DataFormat data_format,
    UT_Array<unsigned char>& frame_data) const
{
    unsigned int num_channels = getCopDataChannelCount(packing);
    for(unsigned int channel_idx = 0; channel_idx < 4; ++channel_idx)
    {
        if(channel_idx < num_channels)
        {
            fpreal value = 0.0f;

            switch(data_format)
            {
                case PXL_FLOAT16:
                {
                    const fpreal16* pixel_cast = (const fpreal16*) pixel_data;
                    value = (fpreal)(*(pixel_cast + channel_idx));
                    break;
                }

                case PXL_FLOAT32:
                {
                    const float* pixel_cast = (const float*) pixel_data;
                    value = *(pixel_cast + channel_idx);
                    break;
                }

                default:
                {
                    break;
                }
            }

            value = SYSclamp(value, 0.0f, 1.0f);

            if(value == 1.0f)
            {
                frame_data.append(255u);
            }
            else
            {
                unsigned int pixel_value = SYSfloor(256.0f * value);
                frame_data.append(pixel_value);
            }
        }
        else
        {
            if(3 == channel_idx)
            {
                frame_data.append(255u);
            }
            else
            {
                frame_data.append(0u);
            }
        }
    }
}


bool
ROP_CopGif::processFrameRaster(fpreal t, TIL_Raster* raster) const
{
    if(!raster || !raster->isValid())
    {
        return false;
    }

    if(!m_width)
    {
        m_width = raster->getXres();
    }

    if(!m_height)
    {
        m_height = raster->getYres();
    }

    if(!m_width || !m_height)
    {
        return false;
    }

    if(m_width != raster->getXres() || m_height != raster->getYres())
    {
        return false;
    }

    // Get raster data format and packing.
    PXL_DataFormat pixel_format = raster->getFormat();
    PXL_Packing packing = raster->getPacking();

    // We only support floating point formats.
    if(!isCopDataFloat(pixel_format))
    {
        return false;
    }

    UT_Array<unsigned char> frame_data;

    for(int idx_width = 0; idx_width < m_width; ++idx_width)
    {
        for(int idx_height = 0; idx_height < m_height; ++idx_height)
        {
            const unsigned char* pixel = (const unsigned char*) raster->getPixel(idx_width, idx_height);
            getFramePixel(pixel, packing, pixel_format, frame_data);
        }
    }

    return true;
}


void
newDriverOperator(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator(ROP_COP_GIF_NAME, ROP_COP_GIF_DESCRIPTION,
        ROP_CopGif::myConstructor, ROP_CopGif::getTemplatePair(), 0, 0,
        ROP_CopGif::getVariablePair(), OP_FLAG_GENERATOR));
}
