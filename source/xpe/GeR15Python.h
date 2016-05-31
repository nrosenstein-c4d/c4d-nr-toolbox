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

#ifndef NR_GER15PYTHON_H
#define NR_GER15PYTHON_H

    #include "misc/legacy.h"
    #include <lib_py.h>

    #if API_VERSION >= 15000
        typedef GePython GeR15Python;
    #else
        class GeR15Python
        {
        public:

            GeR15Python(); //no GIL
            ~GeR15Python(); //no GIL

            Bool Init(const String &handler = String()); //no GIL
            void Free(); //no GIL

            Bool Run(const String& code);

            Bool ImportModule(const String &mod_name); //no GIL

            Bool HasCode() { return _code.Content(); } //no GIL

            Bool CallFunction(const String &name, GeData *ret=nullptr);//GIL
            Bool HasFunction(const String &name);//GIL

            Bool SetString(const String &name, const String &str);//GIL
            Bool SetMatrix(const String &name, const Matrix &m);//GIL
            Bool SetVector(const String &name, const Vector &v);//GIL
            Bool SetReal(const String &name, Real v);//GIL
            Bool SetNode(const String &name, const GeListNode *node);//GIL
            Bool SetContainer(const String &name, const BaseContainer &bc);//GIL
            Bool SetCustom(const String &name, const String &type, const GeData &v);//GIL
            Bool SetLong(const String &name, LONG v);//GIL

            Bool GetContainer(const String &name, BaseContainer &bc);//GIL
            Bool GetLong(const String &name, LONG &v);//GIL
            Bool GetMatrix(const String &name, Matrix &m);//GIL

            // Bool PluginMessage(LONG id, void* data);//GIL

        private:

            PythonLibrary m_PyLib;
            PythonBase *m_pPyBase;

            String _code;
            String _handler;
        };
    #endif

#endif /* NR_GER15PYTHON_H */

