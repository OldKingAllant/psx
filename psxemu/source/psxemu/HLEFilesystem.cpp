#include <psxemu/include/psxemu/HLEFilesystem.hpp>

void psx::kernel::HLEDirectory::DepthVisit(std::function<void(std::shared_ptr<HLEFsEntry>)> visit_function) {
	for (auto& entry : entries) {
		visit_function(entry);
		entry->DepthVisit(visit_function);
	}
}

void psx::kernel::HLEFile::DepthVisit(std::function<void(std::shared_ptr<HLEFsEntry>)> visit_function)
{
	//do nothing
	(void)visit_function;
}
