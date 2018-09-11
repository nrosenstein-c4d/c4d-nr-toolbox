/**
 * Copyright (C) 2013-2015 Niklas Rosenstein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 */

#ifndef NR_XPRESSOEFFECTOR_H
#define NR_XPRESSOEFFECTOR_H

    #include <c4d_baseeffectordata.h>
    #include <c4d_graphview.h>
    #include <c4d_falloffdata.h>

    enum {
        ID_XPRESSOEFFECTOR = 1030784,
        MSG_XPRESSOEFFECTOR_GETNODEMASTER = 1030783, // not used
        MSG_XPRESSOEFFECTOR_GETEXECINFO = 1030786,
    };

    /**
     * This structure contains information for the current execution of
     * an effector. Can be obtain only in call-stack levels below `ModifyPoints()`
     * of the XPresso Effector (eg. from an XPresso Node within the effector).
     */
    struct XPressoEffector_ExecInfo {
        MoData* md;
        BaseObject* gen;
        C4D_Falloff* falloff;
        BaseSelect* sel;

        // Output parameters.
        Bool* easyOn;
        Vector* easyValues;

        XPressoEffector_ExecInfo() { Clean(); }

        void Clean() {
            md = nullptr;
            gen = nullptr;
            falloff = nullptr;
            sel = nullptr;

            easyOn = nullptr;
            easyValues = nullptr;
        }

    };

    class XPressoEffector : public BaseObject {

        XPressoEffector();
        ~XPressoEffector();

    public:

        static XPressoEffector* SafeCast(GeListNode* node) {
            if (!node || !node->IsInstanceOf(ID_XPRESSOEFFECTOR)) return nullptr;
            else return (XPressoEffector*) node;
        }

        Bool GetExecInfo(XPressoEffector_ExecInfo* info) {
            if (info) info->Clean();
            if (!info || !Message(MSG_XPRESSOEFFECTOR_GETEXECINFO, info)) {
                return false;
            }
            return true;
        }

    };

    /**
     * Finds an XPresso Effector in the hierarchy the node might have been
     * invoked from.
     */
    XPressoEffector* GetNodeXPressoEffector(GvNode* node);

#endif /* NR_XPRESSOEFFECTOR_H */
