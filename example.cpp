inline uint64_t bruteforce_directory_base(uint32_t page_id, uint64_t peprocess)
{
	uint64_t base_address = kernel_get_section_base_address(peprocess);
	if (!base_address)
		return 0;

	defines::virtual_address_t virtual_address;
	virtual_address.value = base_address;

	defines::PPHYSICAL_MEMORY_RANGE physical_ranges = reinterpret_cast<defines::PPHYSICAL_MEMORY_RANGE>(kernel_get_physical_ranges());
	for (int i = 0; /**/; i++)
	{
		defines::PHYSICAL_MEMORY_RANGE current_element = { 0 };
		kernel_copy_memory(reinterpret_cast<uint64_t>(&current_element), reinterpret_cast<uint64_t>(&physical_ranges[i]), sizeof(defines::PHYSICAL_MEMORY_RANGE));
		if (!current_element.BaseAddress.QuadPart || !current_element.NumberOfBytes.QuadPart)
			return 0;

		uint64_t current_physical = current_element.BaseAddress.QuadPart;
		for (uint64_t j = 0; j < (current_element.NumberOfBytes.QuadPart / defines::page_size); j++, current_physical += defines::page_size)
		{
			defines::pte_t pml4e = { 0 };
			read_physical_address(page_id, current_physical + 8 * virtual_address.pml4_index, reinterpret_cast<uint64_t>(&pml4e), 8);
			if (!pml4e.present)
				continue;

			defines::pte_t pdpte = { 0 };
			read_physical_address(page_id, (pml4e.page_frame << 12) + 8 * virtual_address.pdpt_index, reinterpret_cast<uint64_t>(&pdpte), 8);
			if (!pdpte.present)
				continue;

			defines::pte_t pde = { 0 };
			read_physical_address(page_id, (pdpte.page_frame << 12) + 8 * virtual_address.pd_index, reinterpret_cast<uint64_t>(&pde), 8);
			if (!pde.present)
				continue;

			defines::pte_t pte = { 0 };
			read_physical_address(page_id, (pde.page_frame << 12) + 8 * virtual_address.pt_index, reinterpret_cast<uint64_t>(&pte), 8);
			if (!pte.present)
				continue;

			uint64_t physical_base = translate_linear_address_legacy(page_id, current_physical, base_address);
			if (!physical_base)
				continue;

			char buffer[sizeof(IMAGE_DOS_HEADER)];
			read_physical_address(page_id, physical_base, reinterpret_cast<uint64_t>(buffer), sizeof(IMAGE_DOS_HEADER));

			PIMAGE_DOS_HEADER header = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer);
			if (header->e_magic != IMAGE_DOS_SIGNATURE)
				continue;

			return current_physical;
		}
	}
}

inline uint64_t translate_linear_address(uint32_t page_id, uint64_t _virtual_address)
{
	defines::virtual_address_t virtual_address;
	virtual_address.value = _virtual_address;

	defines::pte_t pml4e = data::cached_pml4e[virtual_address.pml4_index];
	if (!pml4e.present)
		return 0;

	defines::pte_t pdpte = { 0 };
	read_physical_address(page_id, (pml4e.page_frame << 12) + 8 * virtual_address.pdpt_index, reinterpret_cast<uint64_t>(&pdpte), 8);
	if (!pdpte.present)
		return 0;

	defines::pte_t pde = { 0 };
	read_physical_address(page_id, (pdpte.page_frame << 12) + 8 * virtual_address.pd_index, reinterpret_cast<uint64_t>(&pde), 8);
	if (!pde.present)
		return 0;

	defines::pte_t pte = { 0 };
	read_physical_address(page_id, (pde.page_frame << 12) + 8 * virtual_address.pt_index, reinterpret_cast<uint64_t>(&pte), 8);
	if (!pte.present)
		return 0;

	return (pte.page_frame << 12) + virtual_address.offset;
}

inline bool update_paging_cache()
{
	uint64_t dirbase = bruteforce_directory_base(defines::internal_thread_id, data::target_process_eprocess);
	if (!dirbase)
		return false;

	for (int i = 0; i < 512; i++)
		read_physical_address(defines::internal_thread_id, dirbase + 8 * i, reinterpret_cast<uint64_t>(&data::cached_pml4e[i]), 8);

	return true;
}
