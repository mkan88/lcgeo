//=========================================================================
//  Barrel ECal driver implementation for the CLIC NDM 
//-------------------------------------------------------------------------
//  Baseon on
//   * S. Lu's driver for SiWEcalBarrel, ported from Mokka
//   * M. Frank's generic driver EcalBarrel_geo.cpp
//-------------------------------------------------------------------------
//  D.Protopopescu, Glasgow 
//  M.Frank, CERN
//  S.Lu, DESY
//  $Id$
//=========================================================================

#include "DD4hep/DetFactoryHelper.h"
#include "DD4hep/Printout.h"
#include "XML/Layering.h"
#include "TGeoTrd2.h"

/*--------------------------------------------------
From S.Lu's implementation
include "DDRec/Extensions/LayeringExtensionImpl.h"
include "DDRec/Extensions/SubdetectorExtensionImpl.h"
include "DDRec/Surface.h"
---------------------------------------------------*/

using namespace std;
using namespace DD4hep;
using namespace DD4hep::Geometry;

#define VERBOSE 1

static Ref_t create_detector(LCDD& lcdd, xml_h e, SensitiveDetector sens)  {
  static double tolerance = 0e0;
  Layering      layering (e);
  xml_det_t     x_det     = e;
  Material      air       = lcdd.air();
  int           det_id    = x_det.id();
  string        det_name  = x_det.nameStr();
  xml_comp_t    x_staves  = x_det.staves();
  xml_comp_t    x_dim     = x_det.dimensions();
  int           nsides    = x_dim.numsides();
  double        inner_r   = x_dim.rmin();
  double        dphi      = (2*M_PI/nsides);
  double        hphi      = dphi/2;
  double        mod_z     = layering.totalThickness();
  double        outer_r   = inner_r + mod_z;
  double        totThick  = mod_z;
  DetElement    sdet      (det_name,det_id);
  Volume        motherVol = lcdd.pickMotherVolume(sdet);
  PolyhedraRegular hedra  (nsides,inner_r,inner_r+totThick+tolerance*2e0,x_dim.z());
  Volume        envelope  (det_name,hedra,air);
  PlacedVolume  env_phv   = motherVol.placeVolume(envelope,RotationZYX(0,0,M_PI/nsides));

  env_phv.addPhysVolID("system",det_id);
  env_phv.addPhysVolID("barrel",0);
  sdet.setPlacement(env_phv);

  DetElement    stave_det("stave0",det_id);
  double dx = mod_z / std::sin(dphi); // dx per layer
  dx = 0;
    
  // Must add the 5 alveoli!
  DetElement    alveolus("alveolus0",det_id);
  int n_alveoli = 5;
  double alveoliAirgap = 5.0;

  // Compute the top and bottom face measurements.
  double trd_x2 = (2 * std::tan(hphi) * outer_r - dx)/2 - tolerance;
  double trd_x1 = (2 * std::tan(hphi) * inner_r + dx)/2 - tolerance;
  double trd_y1 = x_dim.z()/2 - tolerance;
  double trd_y2 = trd_y1;
  double trd_z  = mod_z/2 - tolerance;
		
  // Create the trapezoid for the stave.
  Trapezoid trd(trd_x1, // Outer side, i.e. the "short" X side.
		trd_x2, // Inner side, i.e. the "long"  X side.
		trd_y1, // Corresponds to subdetector (or module) Z.
		trd_y2, //
		trd_z); // Thickness, in Y for top stave, when rotated.

  Volume mod_vol("stave",trd,air);

  sens.setType("calorimeter");
  { // =====  buildBarrelStave(lcdd, sens, module_volume) =====
    // Parameters for computing the layer X dimension:
    double stave_z  = trd_y1/2;
    double l_dim_x  = trd_x1/2;                            // Starting X dimension for the layer.
    double adj      = (l_dim_x-trd_x2/2)/2;                // Adjacent angle of triangle.
    double hyp      = std::sqrt(trd_z*trd_z/4 + adj*adj);  // Hypotenuse of triangle.
    double beta     = std::acos(adj / hyp);                // Lower-right angle of triangle.
    double tan_beta = std::tan(beta);                      // Primary coefficient for figuring X.
    double l_pos_z  = -(layering.totalThickness() / 2);

    // Loop over alveoli
    int a_num = 1;

    // Loop over the sets of layer elements in the detector.
    int l_num = 1;
    for(xml_coll_t li(x_det,_U(layer)); li; ++li)  {
#ifdef VERBOSE
      std::cout << "ECalBarrel layer " << l_num << std::endl;
#endif
      xml_comp_t x_layer = li;
      int repeat = x_layer.repeat();
      // Loop over number of repeats for this layer.
      for (int j=0; j<repeat; j++)  {
#ifdef VERBOSE
	std::cout << " ECalBarrel repeat " << j << std::endl;
#endif
	string l_name = _toString(l_num,"layer%d");
	double l_thickness = layering.layer(l_num-1)->thickness();  // Layer's thickness.
	double xcut = (l_thickness / tan_beta);                     // X dimension for this layer.
	l_dim_x -= xcut/2;

	Position   l_pos(0,0,l_pos_z+l_thickness/2);      // Position of the layer.
	Box        l_box(l_dim_x*2-tolerance,stave_z*2-tolerance,l_thickness-tolerance);
	Volume     l_vol(l_name,l_box,air);
	DetElement layer(stave_det, l_name, det_id);

	// Loop over the sublayers or slices for this layer.
	int s_num = 1;
	double s_pos_z = -(l_thickness / 2);
	for(xml_coll_t si(x_layer,_U(slice)); si; ++si)  {
#ifdef VERBOSE
	  std::cout << "  ECalBarrel slice  = " << s_num  << std::endl;
#endif
	  xml_comp_t x_slice = si;
	  string     s_name  = _toString(s_num,"slice%d");
	  double     s_thick = x_slice.thickness();
	  Box        s_box(l_dim_x*2-tolerance,stave_z*2-tolerance,s_thick-tolerance);
	  Volume     s_vol(s_name,s_box,lcdd.material(x_slice.materialStr()));
          DetElement slice(layer,s_name,det_id);

          if ( x_slice.isSensitive() ) {
	    s_vol.setSensitiveDetector(sens);
	  }
          slice.setAttributes(lcdd,s_vol,x_slice.regionStr(),x_slice.limitsStr(),x_slice.visStr());

          // Slice placement.
          PlacedVolume slice_phv = l_vol.placeVolume(s_vol,Position(0,0,s_pos_z+s_thick/2));
          slice_phv.addPhysVolID("slice", s_num);
          slice.setPlacement(slice_phv);
          // Increment Z position of slice.
          s_pos_z += s_thick;
                                        
          // Increment slice number.
          ++s_num;
        }        

        // Set region, limitset, and vis of layer.
        layer.setAttributes(lcdd,l_vol,x_layer.regionStr(),x_layer.limitsStr(),x_layer.visStr());

        PlacedVolume layer_phv = mod_vol.placeVolume(l_vol,l_pos);
        layer_phv.addPhysVolID("layer", l_num);
        layer.setPlacement(layer_phv);
        // Increment to next layer Z position.
        l_pos_z += l_thickness;          
        ++l_num;
      }
    }
  }

  // Set stave visualization.
  if ( x_staves )   {
    mod_vol.setVisAttributes(lcdd.visAttributes(x_staves.visStr()));
  }
  // Phi start for a stave.
  double phi = M_PI / nsides;
  double mod_x_off = dx / 2;             // Stave X offset, derived from the dx.
  double mod_y_off = inner_r + mod_z/2;  // Stave Y offset

  // Loop over alveoli here ?

  // Create nsides staves.
  for (int i = 0; i < nsides; i++, phi -= dphi)      { // i is module number
    // Compute the stave position
    double m_pos_x = mod_x_off * std::cos(phi) - mod_y_off * std::sin(phi);
    double m_pos_y = mod_x_off * std::sin(phi) + mod_y_off * std::cos(phi);
    Transform3D tr(RotationZYX(0,phi,M_PI*0.5),Translation3D(-m_pos_x,-m_pos_y,0));
    PlacedVolume pv = envelope.placeVolume(mod_vol,tr);
    pv.addPhysVolID("system",det_id);
    pv.addPhysVolID("barrel",0);
    pv.addPhysVolID("module",i+1);
    DetElement sd = i==0 ? stave_det : stave_det.clone(_toString(i,"stave%d"));
    sd.setPlacement(pv);
    sdet.add(sd);
#ifdef VERBOSE
    std::cout << "+ Adding ECalBarrel stave " << i  << std::endl;
#endif
  }

  // Set envelope volume attributes.
  envelope.setAttributes(lcdd,x_det.regionStr(),x_det.limitsStr(),x_det.visStr());
  return sdet;
}

/* ----------------------------------- S.Lu's driver ---------------------------------
static Ref_t create_detector_old(LCDD& lcdd, xml_h element, SensitiveDetector sens)  {
  static double tolerance = 0e0;

  xml_det_t     x_det     = element;
  string        det_name  = x_det.nameStr();
  Layering      layering (element);

  Material      air       = lcdd.air();
  Material      vacuum    = lcdd.vacuum();

  int           det_id    = x_det.id();
  xml_comp_t    x_staves  = x_det.staves();
  DetElement    sdet      (det_name,det_id);
  Volume        motherVol = lcdd.pickMotherVolume(sdet);

  xml_comp_t    x_dim     = x_det.dimensions();
  int           nsides    = x_dim.numsides();
  double        inner_r   = x_dim.rmin();
  double        dphi      = (2*M_PI/nsides);
  double        hphi      = dphi/2;
  double        outer_r   = x_dim.rmax();
  PolyhedraRegular hedra  (nsides,inner_r,outer_r+tolerance*2e0,x_dim.z()*2.0);
  Volume        envelope  (det_name+"_envelope",hedra,air);
  PlacedVolume  env_phv   = motherVol.placeVolume(envelope,RotationZYX(0,0,M_PI/nsides));

  env_phv.addPhysVolID("system",det_id);
  sdet.setPlacement(env_phv);

  sens.setType("calorimeter");

  DetElement    stave_det("module0stave0",det_id);


//====================================================================
//
// Read all the constant from ILD_o1_v05.xml
// Use them to build HcalBarrel
//
//====================================================================

  int N_FIBERS_W_STRUCTURE = 2; 
  int N_FIBERS_ALVOULUS = 3;

  //  read parametere from compact.xml file
  double Ecal_Alveolus_Air_Gap              = lcdd.constant<double>("Ecal_Alveolus_Air_Gap");
  double Ecal_Slab_shielding                = lcdd.constant<double>("Ecal_Slab_shielding");
  double Ecal_Slab_copper_thickness         = lcdd.constant<double>("Ecal_Slab_copper_thickness");
  double Ecal_Slab_PCB_thickness            = lcdd.constant<double>("Ecal_Slab_PCB_thickness");
  double Ecal_Slab_glue_gap                 = lcdd.constant<double>("Ecal_Slab_glue_gap");
  double Ecal_Slab_ground_thickness         = lcdd.constant<double>("Ecal_Slab_ground_thickness");
  double Ecal_fiber_thickness               = lcdd.constant<double>("Ecal_fiber_thickness");
  double Ecal_Si_thickness                  = lcdd.constant<double>("Ecal_Si_thickness");
  
  double Ecal_inner_radius                  = lcdd.constant<double>("TPC_outer_radius") +lcdd.constant<double>("Ecal_Tpc_gap");
  double Ecal_radiator_thickness1           = lcdd.constant<double>("Ecal_radiator_layers_set1_thickness");
  double Ecal_radiator_thickness2           = lcdd.constant<double>("Ecal_radiator_layers_set2_thickness");
  double Ecal_radiator_thickness3           = lcdd.constant<double>("Ecal_radiator_layers_set3_thickness");
  double Ecal_Barrel_halfZ                  = lcdd.constant<double>("Ecal_Barrel_halfZ");
  
  double Ecal_support_thickness             = lcdd.constant<double>("Ecal_support_thickness");
  double Ecal_front_face_thickness          = lcdd.constant<double>("Ecal_front_face_thickness");
  double Ecal_lateral_face_thickness        = lcdd.constant<double>("Ecal_lateral_face_thickness");
  double Ecal_Slab_H_fiber_thickness        = lcdd.constant<double>("Ecal_Slab_H_fiber_thickness");

  double Ecal_Slab_Sc_PCB_thickness         = lcdd.constant<double>("Ecal_Slab_Sc_PCB_thickness");
  double Ecal_Sc_thickness                  = lcdd.constant<double>("Ecal_Sc_thickness");
  double Ecal_Sc_reflector_thickness        = lcdd.constant<double>("Ecal_Sc_reflector_thickness");

  int    Ecal_nlayers1                      = lcdd.constant<int>("Ecal_nlayers1");
  int    Ecal_nlayers2                      = lcdd.constant<int>("Ecal_nlayers2");
  int    Ecal_nlayers3                      = lcdd.constant<int>("Ecal_nlayers3");
  int    Ecal_barrel_number_of_towers       = lcdd.constant<int>("Ecal_barrel_number_of_towers");
  
  
//====================================================================
//
// general calculated parameters
//
//====================================================================
  
  double Ecal_total_SiSlab_thickness = 
    Ecal_Slab_shielding + 
    Ecal_Slab_copper_thickness + 
    Ecal_Slab_PCB_thickness +
    Ecal_Slab_glue_gap + 
    Ecal_Si_thickness + 
    Ecal_Slab_ground_thickness +
    Ecal_Alveolus_Air_Gap / 2;
#ifdef VERBOSE
  std::cout << " Ecal_total_SiSlab_thickness = " << Ecal_total_SiSlab_thickness  << std::endl;
#endif
  
  

  double Ecal_total_ScSlab_thickness = 
    Ecal_Slab_shielding + 
    Ecal_Slab_copper_thickness + 
    Ecal_Slab_Sc_PCB_thickness +
    Ecal_Sc_thickness + 
    Ecal_Sc_reflector_thickness * 2 +
    Ecal_Alveolus_Air_Gap / 2;
#ifdef VERBOSE
  std::cout << " Ecal_total_ScSlab_thickness = " << Ecal_total_ScSlab_thickness  << std::endl;
#endif
  

    int Number_of_Si_Layers_in_Barrel = 0;
    int Number_of_Sc_Layers_in_Barrel = 0;


#ifdef VERBOSE
  std::cout << " Ecal total number of Silicon layers = " << Number_of_Si_Layers_in_Barrel  << std::endl;
  std::cout << " Ecal total number of Scintillator layers = " << Number_of_Sc_Layers_in_Barrel  << std::endl;
#endif
  
  // In this release the number of modules is fixed to 5
  double Ecal_Barrel_module_dim_z = 2 * Ecal_Barrel_halfZ / 5. ;
#ifdef VERBOSE
  std::cout << "Ecal_Barrel_module_dim_z  = " << Ecal_Barrel_module_dim_z  << std::endl;
#endif

  // The alveolus size takes in account the module Z size
  // but also 4 fiber layers for the alveoulus wall, the all
  // divided by the number of towers
  double alveolus_dim_z = 
    (Ecal_Barrel_module_dim_z - 2. * Ecal_lateral_face_thickness) /
    Ecal_barrel_number_of_towers - 
    2 * N_FIBERS_ALVOULUS  * Ecal_fiber_thickness  - 
    2 * Ecal_Slab_H_fiber_thickness -
    2 * Ecal_Slab_shielding;


#ifdef VERBOSE
  std::cout << "alveolus_dim_z = " <<  alveolus_dim_z << std::endl;
#endif

  int n_total_layers = Ecal_nlayers1 + Ecal_nlayers2 + Ecal_nlayers3;
  Number_of_Si_Layers_in_Barrel = n_total_layers+1;

  double module_thickness = 
    Ecal_nlayers1 * Ecal_radiator_thickness1 +
    Ecal_nlayers2 * Ecal_radiator_thickness2 +
    Ecal_nlayers3 * Ecal_radiator_thickness3 +
    
    int(n_total_layers/2) * // fiber around W struct layers
    (N_FIBERS_W_STRUCTURE * 2 *  Ecal_fiber_thickness) +
    
    Number_of_Si_Layers_in_Barrel * // Silicon slabs plus fiber around and inside
    (Ecal_total_SiSlab_thickness +
     (N_FIBERS_ALVOULUS + 1 ) * Ecal_fiber_thickness) +
    
    Number_of_Sc_Layers_in_Barrel * // Scintillator slabs plus fiber around and inside
    (Ecal_total_ScSlab_thickness +
     (N_FIBERS_ALVOULUS + 1 ) * Ecal_fiber_thickness) +
    
    Ecal_support_thickness + Ecal_front_face_thickness;
  
  //#ifdef VERBOSE
  std::cout << "For information : module_thickness = " << module_thickness  << std::endl;
  //#endif
  
  // module barrel key parameters
  double  bottom_dim_x = 2. * tan(M_PI/8.) * Ecal_inner_radius +
    module_thickness/sin(M_PI/4.);
  
  double top_dim_x = bottom_dim_x - 2 * module_thickness;
  
  
#ifdef VERBOSE
  std::cout << " bottom_dim_x = " << bottom_dim_x  << std::endl;
  std::cout << " top_dim_x = " << top_dim_x << std::endl;
  std::cout << " Ecal total number of Silicon layers = " << Number_of_Si_Layers_in_Barrel  << std::endl;
  std::cout << " Ecal total number of Scintillator layers = " << Number_of_Sc_Layers_in_Barrel  << std::endl;
#endif
  


// ========= Create Ecal Barrel stave   ====================================
//  It will be the volume for palcing the Ecal Barrel alveolus(i.e. Layers).
//  And the structure W plate.
//  Itself will be placed into the world volume.
// ==========================================================================

  // The TOP_X and BOTTOM_X is different in Mokka and DD4hep
  Trapezoid trd(top_dim_x / 2,
		bottom_dim_x / 2, 
		Ecal_Barrel_module_dim_z / 2,
		Ecal_Barrel_module_dim_z / 2,
		module_thickness/2);

  Volume mod_vol(det_name+"_module",trd,air);



  // We count the layers starting from IP and from 1,
  // so odd layers should be inside slabs and
  // even ones on the structure.
  // The structure W layers are here big plans, as the 
  // gap between each W plate is too small to create problems 
  // The even W layers are part of H structure placed inside
  // the alveolus.

  // ############################
  //  Dimension of radiator wLog
  //  slice provide the thickness
  // ############################


  double y_floor = 
    Ecal_front_face_thickness +
    N_FIBERS_ALVOULUS * Ecal_fiber_thickness;


  // ############################
  //  Dimension of alveolus
  //  slice provide the thickness
  // ############################
  
    // =====  build Si Slab and put into the Layer volume =====
    // =====  place the layer into the module 5 time for one full layer into the trd module ====
    // =====  build and place barrel structure into trd module ====
    // Parameters for computing the layer X dimension:
    double stave_z  =(Ecal_Barrel_module_dim_z - 2. * Ecal_lateral_face_thickness) / Ecal_barrel_number_of_towers/2.;
    double l_dim_x  = bottom_dim_x/2.;                            // Starting X dimension for the layer.
    double l_pos_z  = module_thickness/2;

    l_dim_x -= y_floor;
    l_pos_z -= y_floor;


    // ------------- create extension objects for reconstruction -----------------
    DDRec::LayeringExtensionImpl* layeringExtension = new DDRec::LayeringExtensionImpl ;
    DDRec::SubdetectorExtensionImpl* subDetExtension = new DDRec::SubdetectorExtensionImpl( sdet )  ;
    Position layerNormal(0,0,1); //fg: defines direction of thickness in Box for layer slices
    
    subDetExtension->setIsBarrel(true) ;
    subDetExtension->setNSides( 8 ) ;
    //    subDetExtension->setPhi0( 0 ) ;
    subDetExtension->setRMin( Ecal_inner_radius ) ;
    subDetExtension->setRMax( ( Ecal_inner_radius + module_thickness ) / cos( M_PI/8. ) ) ;
    subDetExtension->setZMin( 0. ) ;
    subDetExtension->setZMax( Ecal_Barrel_halfZ ) ;
    

    // base vectors for surfaces:
    DDSurfaces::Vector3D u(1,0,0) ;
    DDSurfaces::Vector3D v(0,1,0) ;
    DDSurfaces::Vector3D n(0,0,1) ;

    //------------------------------------------------------------------------------------

    //-------------------- start loop over ECAL layers ----------------------
    // Loop over the sets of layer elements in the detector.
    int l_num = 1;
    for(xml_coll_t li(x_det,_U(layer)); li; ++li)  {
      xml_comp_t x_layer = li;
      int repeat = x_layer.repeat();
      // Loop over number of repeats for this layer.
      for (int j=0; j<repeat; j++)    {
	string l_name = _toString(l_num,"layer%d");
	double l_thickness = layering.layer(l_num-1)->thickness();  // Layer's thickness.
	double xcut = (l_thickness);                     // X dimension for this layer.
	l_dim_x -= xcut;


	Box        l_box(l_dim_x-tolerance,stave_z-tolerance,l_thickness/2.0-tolerance);
	Volume     l_vol(det_name+"_"+l_name,l_box,air);

	l_vol.setVisAttributes(lcdd.visAttributes(x_layer.visStr()));

	//fg: need vector of DetElements for towers ! 
	//    DetElement layer(stave_det, l_name, det_id);
	std::vector< DetElement > layers( Ecal_barrel_number_of_towers )  ;
	
	// place layer 5 times in module. at same layer position (towers !)
	double l_pos_y = Ecal_Barrel_module_dim_z / 2.;
       	for (int i=0; i<Ecal_barrel_number_of_towers; i++){ // need four clone

	  layers[i] = DetElement( stave_det, l_name+_toString(i,"tower%02d") , det_id ) ;

	  l_pos_y -= stave_z;
	  Position   l_pos(0,l_pos_y,l_pos_z-l_thickness/2.);      // Position of the layer.
	  PlacedVolume layer_phv = mod_vol.placeVolume(l_vol,l_pos);
	  layer_phv.addPhysVolID("layer", l_num);
	  layer_phv.addPhysVolID("tower", i);

	  layers[i].setPlacement(layer_phv);
	  l_pos_y -= stave_z;
	}




	// Loop over the sublayers or slices for this layer.
	int s_num = 1;
	double s_pos_z = -(l_thickness / 2);


	//--------------------------------------------------------------------------------
	// BuildBarrelAlveolus: BuildSiliconSlab:
	//--------------------------------------------------------------------------------
	double radiator_dim_y = -1.0; //to be updated with slice radiator thickness 

	

	for(xml_coll_t si(x_layer,_U(slice)); si; ++si)  {
	  xml_comp_t x_slice = si;
	  string     s_name  =  _toString(s_num,"slice%d");
	  double     s_thick = x_slice.thickness();

	  std::cout<<"Ecal_barrel_number_of_towers: "<< Ecal_barrel_number_of_towers <<std::endl;

	  double slab_dim_x = l_dim_x-tolerance;
	  double slab_dim_y = s_thick/2.;
	  double slab_dim_z = stave_z-tolerance;

	  Box        s_box(slab_dim_x,slab_dim_z,slab_dim_y);
	  Volume     s_vol(det_name+"_"+l_name+"_"+s_name,s_box,lcdd.material(x_slice.materialStr()));
	  //fg: not needed          DetElement slice(layer,s_name,det_id);

	  s_vol.setVisAttributes(lcdd.visAttributes(x_slice.visStr()));

	  std::cout<<"x_slice.materialStr(): "<< x_slice.materialStr() <<std::endl;
	  if (x_slice.materialStr().compare(x_staves.materialStr()) == 0)
	    radiator_dim_y = s_thick;
	  // W StructureLayer has the same thickness as W radiator layer in the Alveolus layer

          if ( x_slice.isSensitive() ) {
	    s_vol.setSensitiveDetector(sens);
	  }
	  //fg: not needed   slice.setAttributes(lcdd,s_vol,x_slice.regionStr(),x_slice.limitsStr(),x_slice.visStr());

          // Slice placement.
          PlacedVolume slice_phv = l_vol.placeVolume(s_vol,Position(0,0,s_pos_z+s_thick/2));

          if ( x_slice.isSensitive() ) {

	    slice_phv.addPhysVolID("slice",s_num);

	    // add a measurement surface to the layer for every sensitive slice:
	    DDRec::VolPlane surf( s_vol , DDSurfaces::SurfaceType(DDSurfaces::SurfaceType::Sensitive) , slab_dim_y , slab_dim_y , u,v,n ) ; //,o ) ;

	    // add them to the layers of all towers
	    for (int i=0; i<Ecal_barrel_number_of_towers; i++){
	      DDRec::volSurfaceList(  layers[i] )->push_back(  surf ) ;
	    }
	  }

	  //fg: not needed   slice.setPlacement(slice_phv);

          // Increment Z position of slice.
          s_pos_z += s_thick;
                                        
          // Increment slice number.
          ++s_num;
        }        

	
	if(radiator_dim_y <= 0) {
	  stringstream err;
	  err << " \n ERROR: The subdetector " << x_det.nameStr() << " geometry parameter -- radiator_dim_y = " << radiator_dim_y ;
	  err << " \n Please check the radiator material name in the subdetector xml file";
	  throw runtime_error(err.str());
	}

	// #########################
	// BuildBarrelStructureLayer
	// #########################


	l_dim_x -=  (radiator_dim_y +  Ecal_fiber_thickness * (N_FIBERS_ALVOULUS + N_FIBERS_W_STRUCTURE));
	double radiator_dim_x = l_dim_x*2.;

#ifdef VERBOSE
	std::cout << "radiator_dim_x = " << radiator_dim_x << std::endl;
#endif  

	double radiator_dim_z =
	  Ecal_Barrel_module_dim_z -
	  2 * Ecal_lateral_face_thickness -
	  2 * N_FIBERS_W_STRUCTURE * Ecal_fiber_thickness;
	
	string bs_name="bs";
	
	Box        barrelStructureLayer_box(radiator_dim_x/2.,radiator_dim_z/2.,radiator_dim_y/2.);
	Volume     barrelStructureLayer_vol(det_name+"_"+l_name+"_"+bs_name,barrelStructureLayer_box,lcdd.material(x_staves.materialStr()));

	barrelStructureLayer_vol.setVisAttributes(lcdd.visAttributes(x_layer.visStr()));	

	
	//fg: now done above before slice loop
	// // place 5 times in module. at same layer position.
	// double l_pos_y = Ecal_Barrel_module_dim_z / 2.;
	// for (int i=0; i<Ecal_barrel_number_of_towers; i++){ // need four clone
	//   l_pos_y -= stave_z;
	//   Position   l_pos(0,l_pos_y,l_pos_z-l_thickness/2.);      // Position of the layer.
	//   PlacedVolume layer_phv = mod_vol.placeVolume(l_vol,l_pos);
	//   layer_phv.addPhysVolID("layer", l_num);
	//   layer_phv.addPhysVolID("tower", i);
	//   layer.setPlacement(layer_phv);
	//   l_pos_y -= stave_z;
	// }


        // Increment to next layer Z position.
        l_pos_z -= l_thickness;          

	// Without last W StructureLayer, the last part is Si SD even layer.
	// the last number of  Ecal_nlayers1, Ecal_nlayers2 and  Ecal_nlayers3 is odd.
	int even_layer = l_num*2;
	if(even_layer > Ecal_nlayers1 + Ecal_nlayers2 + Ecal_nlayers3) continue;
	//if ( Number_of_Si_Layers_in_Barrel > n_total_layers ) continue;

	double bsl_pos_z = l_pos_z - (radiator_dim_y/2. + Ecal_fiber_thickness * (N_FIBERS_ALVOULUS + N_FIBERS_W_STRUCTURE));
	l_pos_z -= (Ecal_fiber_thickness * (N_FIBERS_ALVOULUS + N_FIBERS_W_STRUCTURE));

	Position   bsl_pos(0,0,bsl_pos_z);      // Position of the layer.
	PlacedVolume  barrelStructureLayer_phv = mod_vol.placeVolume(barrelStructureLayer_vol,bsl_pos);

	l_dim_x -=  (Ecal_fiber_thickness * (N_FIBERS_ALVOULUS + N_FIBERS_W_STRUCTURE));	
	l_pos_z -= (radiator_dim_y + Ecal_fiber_thickness * (N_FIBERS_ALVOULUS + N_FIBERS_W_STRUCTURE));
	
        ++l_num;
      }
    }
  

    // Set stave visualization.
    if (x_staves)   {
      mod_vol.setVisAttributes(lcdd.visAttributes(x_staves.visStr()));
    }
    
    
    
//====================================================================
// Place ECAL Barrel stave module into the envelope volume
//====================================================================
    double X,Y;
    X = module_thickness * sin(M_PI/4.);
    Y = Ecal_inner_radius + module_thickness / 2.;
    
    for (int stave_id = 1; stave_id <= nsides ; stave_id++)
      for (int module_id = 1; module_id < 6; module_id++)
	{
	  double phirot =  (stave_id-1) * dphi - hphi;
	  double module_z_offset =  (2 * module_id-6) * Ecal_Barrel_module_dim_z/2.;
	  
	  // And the rotation in Mokka is right hand rule, and the rotation in DD4hep is clockwise rule
	  // So there is a negitive sign when port Mokka into DD4hep 
	  Transform3D tr(RotationZYX(0,phirot,M_PI*0.5),Translation3D(X*cos(phirot)-Y*sin(phirot),
								      X*sin(phirot)+Y*cos(phirot),
								      module_z_offset));
	  PlacedVolume pv = envelope.placeVolume(mod_vol,tr);
	  pv.addPhysVolID("system",det_id);
	  pv.addPhysVolID("module",module_id);
	  pv.addPhysVolID("stave",stave_id);
	  DetElement sd = (module_id==0&&stave_id==0) ? stave_det : stave_det.clone(_toString(module_id,"module%d")+_toString(stave_id,"stave%d"));
	  sd.setPlacement(pv);
	  sdet.add(sd);
	  
	}
    
    // Set envelope volume attributes.
    envelope.setAttributes(lcdd,x_det.regionStr(),x_det.limitsStr(),x_det.visStr());
    
    sdet.addExtension< DDRec::LayeringExtension >( layeringExtension ) ;
    sdet.addExtension< DDRec::SubdetectorExtension >( subDetExtension ) ;


    return sdet;
}
---------------------------------------------------- */

DECLARE_DETELEMENT(ECalBarrel_o1_v01,create_detector)