test_d3d9ex_deps = [ lib_d3d9, lib_d3dcompiler_47 ]

args = {
  'dependencies': test_d3d9ex_deps,
  'install': true
}

executable('d3d9ex-triangle', files('d3d9ex_triangle.cpp'), kwargs: args)
executable('d3d9ex-present', files('d3d9ex_present.cpp'), kwargs: args)
executable('d3d9ex-nv12', files('d3d9ex_nv12.cpp'), kwargs: args)
executable('d3d9ex-module-refs', files('d3d9ex_module_refs.cpp'), kwargs: args)
