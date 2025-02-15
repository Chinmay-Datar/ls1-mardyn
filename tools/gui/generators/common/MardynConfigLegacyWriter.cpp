/*
 * MardynConfigLegacyWriter.cpp
 *
 * @Date: 13.09.2011
 * @Author: eckhardw
 */

#include "MardynConfigLegacyWriter.h"
#include "MardynConfiguration.h"
#include <fstream>

using namespace std;

void writeOutputConfig(ofstream& output, const OutputConfiguration& config) {
	output << "output " << config.getName() << " " << config.getOutputFrequency() << " " << config.getOutputPrefix() << endl;
}

MardynConfigLegacyWriter::MardynConfigLegacyWriter() {
}

MardynConfigLegacyWriter::~MardynConfigLegacyWriter() {
}

void MardynConfigLegacyWriter::writeConfigFile(const std::string& directory, const std::string& fileName,
		const MardynConfiguration& config) {

	string fullName = directory + "/" + fileName;
	ofstream output(fullName.c_str());
	output << "MDProjectConfig\n" << endl;

	output << "# THIS CONFIGURATION WAS GENERATED BY THE SCENARIO-GENERATOR!\n" << endl;

	output << "timestepLength " << config.getTimestepLength() << endl;
	output << "cutoffRadius " << config.getCutoffRadius() << endl;
	output << "LJCutoffRadius " << config.getLJCutoffRadius() << endl;
	output << "phaseSpaceFile OldStyle " << config.getScenarioName() << ".inp" << endl;
	if (config.getParallelisationTypeString() != MardynConfiguration::ParallelisationType_NONE) {
		output << "parallelization " << config.getParallelisationTypeString() << endl;
	}
	output << "datastructure " << config.getContainerTypeString() << " 1" << endl;

	output << endl;
	if (config.isNVE()) {
		output << "NVE" << endl << endl;
	}

	if (config.hasResultWriter()) {
		writeOutputConfig(output, config.getResultWriterConfig());
	}
	if (config.hasStatisticsWriter()) {
		writeOutputConfig(output, config.getStatisticsWriterConfig());
	}
	if (config.hasVTKMoleculeWriter()) {
		writeOutputConfig(output, config.getVtkMoleculeWriterConfig());
	}
	if (config.hasVTKGridWriter()) {
		writeOutputConfig(output, config.getVtkGridWriterConfig());
	}
	output.close();
}
