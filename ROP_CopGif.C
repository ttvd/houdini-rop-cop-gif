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


#define ROP_COP_GIF_COP_PATH "cop_path"
#define ROP_COP_GIF_OUTPUT_FILE "output_file"
#define ROP_COP_GIF_SEPARATOR "rop_cop_gif_separator"


static PRM_Name s_name_file_output(ROP_COP_GIF_OUTPUT_FILE, "Output File");
static PRM_Name s_name_cop_path(ROP_COP_GIF_COP_PATH, "COP Path");
static PRM_Name s_name_separator(ROP_COP_GIF_SEPARATOR, "");

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

    const int template_count = 4;
    ROP_CopGif::ms_template = new PRM_Template[template_count];
    int template_idx = 0;

    ROP_CopGif::ms_template[template_idx++] = PRM_Template(PRM_SEPARATOR, 0, &s_name_separator);

    ROP_CopGif::ms_template[template_idx++] =
        PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &s_name_cop_path, 0, 0, 0, 0, &PRM_SpareData::cop2Path),

    ROP_CopGif::ms_template[template_idx++] =
        PRM_Template(PRM_FILE, 1, &s_name_file_output, 0, 0, 0, 0, &s_spare_file_output);

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
    TIL_Raster* raster = m_cop_resolver->getNodeRaster(cop_full_path, "C", "");
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

    // Process raster.

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

    UT_String string_result = "";

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
newDriverOperator(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator(ROP_COP_GIF_NAME, ROP_COP_GIF_DESCRIPTION,
        ROP_CopGif::myConstructor, ROP_CopGif::getTemplatePair(), 0, 0,
        ROP_CopGif::getVariablePair(), OP_FLAG_GENERATOR));
}
