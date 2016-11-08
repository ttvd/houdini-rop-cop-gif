#pragma once

#include <ROP/ROP_Node.h>


class GU_Detail;


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

        //! Called at the beginning of rendering to perform any intialization necessary.
        virtual int startRender(int nframes, fpreal time_start, fpreal time_end);

        //! Called once for every frame that is rendered.
        virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);

        //! Called after the rendering is done to perform any post-rendering steps required.
        virtual ROP_RENDER_CODE endRender();

        //! Called to enable / disable parameters.
        virtual bool updateParmsFlags();

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

    protected:

        //! Time for frames start and end.
        fpreal m_render_time_start;
        fpreal m_render_time_end;
};
