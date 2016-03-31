/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "vm_core.h"

#include "StartupManagerImpl.hpp"
#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConfigurationLanguageInterfaceImpl.hpp"
#include "ConfigurationFlat.hpp"
#include "VerboseManagerImpl.hpp"

#define OMR_XMALLOCGC "-Xmallocgc"
#define OMR_XMALLOCGC_LENGTH sizeof(OMR_XMALLOCGC)

bool
MM_StartupManagerImpl::handleOption(MM_GCExtensionsBase *extensions, char *option)
{

	bool result = MM_StartupManager::handleOption(extensions, option);

	if (!result) {
		if (0 == strncmp(option, OMR_XMALLOCGC, OMR_XMALLOCGC_LENGTH)) {
			rb_vm_t *vm = (rb_vm_t *) extensions->getOmrVM()->_language_vm;
			vm->enableMallocGC = 1;
			result = true;
		}
	}

	return result;
}

MM_ConfigurationLanguageInterface *
MM_StartupManagerImpl::createConfigurationLanguageInterface(MM_EnvironmentBase *env)
{
	return MM_ConfigurationLanguageInterfaceImpl::newInstance(env);
}

MM_CollectorLanguageInterface *
MM_StartupManagerImpl::createCollectorLanguageInterface(MM_EnvironmentBase *env)
{
	return MM_CollectorLanguageInterfaceImpl::newInstance(env);
}

MM_VerboseManagerBase *
MM_StartupManagerImpl::createVerboseManager(MM_EnvironmentBase* env)
{
	return MM_VerboseManagerImpl::newInstance(env, env->getOmrVM());
}

char *
MM_StartupManagerImpl::getOptions(void)
{
	char *options = getenv("OMR_GC_OPTIONS");
	return options;
}

MM_Configuration *
MM_StartupManagerImpl::createConfiguration(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface *cli)
{
	return MM_ConfigurationFlat::newInstance(env, cli);
}

