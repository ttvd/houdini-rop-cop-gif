#pragma once

#include <ROP/ROP_Node.h>
#include <PXL/PXL_Common.h>

#include <UT/UT_Array.h>

#include "gif.h"


class TIL_CopResolver;
class TIL_Raster;


class ROP_CopGif : public ROP_Node
{
    public:

        //! Provides access to our parm templates.
        static OP_TemplatePair* getTemplatePair();

        //! Provides access to our variables.
        static OP_VariablePair* getVariablePair();

        //! Creates an instance of this node.
        static OP_Node* myConstructor(OP_Network* net, const char* name, OP_Operator* op);

    protected:

        ROP_CopGif(OP_Network* net, const char* name, OP_Operator* entry);
        virtual ~ROP_CopGif();

    protected:

        //! Called at the beginning of rendering to perform any initialization necessary.
        virtual int startRender(int num_frames, fpreal time_start, fpreal time_end);

        //! Called once for every frame that is rendered.
        virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);

        //! Called after the rendering is done to perform any post-rendering steps required.
        virtual ROP_RENDER_CODE endRender();

        //! Called to enable / disable parameters.
        virtual bool updateParmsFlags();

    protected:

        //! Helper function which retrieves a full path to a given cop2 node.
        bool getFullCopPath(const UT_String& relative_path, UT_String& full_path) const;

        //! Helper function to get number of channels in packing.
        unsigned int getCopDataChannelCount(PXL_Packing packing) const;

        //! Return true if given data format is a floating point one.
        bool isCopDataFloat(PXL_DataFormat data_format) const;

        //! Helper function to get an RGBA value from a given pixel value.
        void getFramePixel(const unsigned char* pixel_data, PXL_Packing packing, PXL_DataFormat data_format,
            UT_Array<unsigned char>& frame_data) const;

        //! Helper function which processes the given raster and extracts raw data.
        bool processFrameRaster(fpreal t, TIL_Raster* raster) const;

    public:

        //! Templates.
        static PRM_Template* ms_template;
        static OP_TemplatePair* ms_template_pair;
        static OP_VariablePair* ms_variable_pair;

    protected:

        //! Parameter evaluation - retrieve cop node path.
        void getParamCopPath(fpreal t, UT_String& cop_path) const;

        //! Parameter evaluation - retrieve output fule name.
        void getParamOutputFileName(fpreal t, UT_String& output_file) const;

        //! Parameter evaluation - retrieve the name of the image plane.
        void getParamImagePlaneName(fpreal t, UT_String& image_plane_name) const;

        //! Parameter evaluation - return true if we need to dither gif output.
        bool getParamDither(fpreal t) const;

    protected:

        //! Time for frames start and end.
        fpreal m_render_time_start;
        fpreal m_render_time_end;

    protected:

        //! List of frames.
        mutable UT_Array<UT_Array<unsigned char> > m_frames;

        //! Width of data (in pixels).
        mutable unsigned int m_width;

        //! Height of data (in pixels).
        mutable unsigned int m_height;

    protected:

        //! COP2 resolver.
        TIL_CopResolver* m_cop_resolver;
};
