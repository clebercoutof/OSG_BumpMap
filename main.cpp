#include <osg/Group>
#include <osg/Image>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/StateSet>
#include <osg/TexMat>
#include <osg/TexEnv>
#include <osg/Texture2D>
#include <osg/ShapeDrawable>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Uniform>

#include <osgFX/BumpMapping>

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>

#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>

#include <osgUtil/Optimizer>

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#define TEXTURE_NUM 28

const int TEXTURE_UNIT_SHADER_SPECULAR = 2;
const int TEXTURE_UNIT_SHADER_NORMAL = 1;
const int TEXTURE_UNIT_SHADER_DIFFUSE = 0;

osg::Geometry* scale_texture(osg::Geometry *geometry, double scale_x, double scale_y, const int texture_unit_diffuse, const int texture_unit_normal =
        -1, const int texture_unit_specular = -1) {

    osg::Vec2Array* tex_coord = dynamic_cast<osg::Vec2Array*>(geometry->getTexCoordArray(0));
    if(tex_coord)
    for (unsigned int i = 0; i < tex_coord->getNumElements(); ++i)
        (*tex_coord)[i].set((*tex_coord)[i].x() * scale_x, (*tex_coord)[i].y() * scale_y);
    else{
      std::cout << "MISS TEXTURE COORDINATE VECTOR " << std::endl;
      exit(0);
    }

    if (tex_coord) {
        geometry->setTexCoordArray(texture_unit_diffuse, tex_coord);
        if (texture_unit_normal > -1)
            geometry->setTexCoordArray(TEXTURE_UNIT_SHADER_NORMAL, tex_coord);
        // if (texture_unit_specular > -1)
        //     geometry->setTexCoordArray(TEXTURE_UNIT_SHADER_SPECULAR, tex_coord);
    } else {
        std::cout << "MISS TEXTURE COORDINATE " << std::endl;
        exit(0);
    }

    return geometry;
}

osg::StateSet* createShaderBumpMap(const int texture_unit_diffuse, const int texture_unit_normal, const int texture_unit_specular,
        osg::StateSet* temp_state = 0) {

    if (temp_state == 0)
        temp_state = new osg::StateSet();

    osg::Program* bumpMapProgramObject = new osg::Program;

    // Set shader
    osg::Shader* bumpVertexObject = new osg::Shader(osg::Shader::VERTEX);
    osg::Shader* bumpFragmentObject = new osg::Shader(osg::Shader::FRAGMENT);

    std::string bumpVertexFileName = osgDB::findDataFile("bump_map.vert");
    bumpVertexObject->loadShaderSourceFromFile(bumpVertexFileName.c_str());
    std::string bumpFragmentFileName = osgDB::findDataFile("bump_map.frag");
    bumpFragmentObject->loadShaderSourceFromFile(bumpFragmentFileName.c_str());

    bumpMapProgramObject->addShader(bumpVertexObject);
    bumpMapProgramObject->addShader(bumpFragmentObject);

    temp_state->addUniform(new osg::Uniform("normalTexture", texture_unit_normal));
    temp_state->addUniform(new osg::Uniform("diffuseTexture", texture_unit_diffuse));
    temp_state->addUniform(new osg::Uniform("specularTexture", texture_unit_specular));
    temp_state->setAttributeAndModes(bumpMapProgramObject, osg::StateAttribute::ON);
    temp_state->setDataVariance(osg::Object::STATIC);

    return temp_state;
}

//std::vector<osg::ref_ptr<osg::Texture2D> > normal_textures, difuse_textures, specular_textures;

bool writeOSGFileFromNode(osg::Node &node, std::string out_file = "out.osg") {

    return osgDB::writeNodeFile(node, out_file);
}

osg::Geometry* createSquare(float textureCoordMax = 1.0f) {
    // set up the Geometry.
    osg::Geometry* geom = new osg::Geometry;

    osg::Vec3Array* coords = new osg::Vec3Array(4);
    (*coords)[0].set(-1.0f, 0.0f, 1.0f);
    (*coords)[1].set(-1.0f, 0.0f, -1.0f);
    (*coords)[2].set(1.0f, 0.0f, -1.0f);
    (*coords)[3].set(1.0f, 0.0f, 1.0f);
    geom->setVertexArray(coords);

    osg::Vec3Array* norms = new osg::Vec3Array(1);
    (*norms)[0].set(0.0f, -1.0f, 0.0f);
    geom->setNormalArray(norms, osg::Array::BIND_OVERALL);

    osg::Vec2Array* tcoords = new osg::Vec2Array(4);
    (*tcoords)[0].set(0.0f, 0.0f);
    (*tcoords)[1].set(0.0f, textureCoordMax);
    (*tcoords)[2].set(textureCoordMax, textureCoordMax);
    (*tcoords)[3].set(textureCoordMax, 0.0f);
    geom->setTexCoordArray(0, tcoords);

    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

    return geom;
}

osg::ref_ptr<osg::StateSet> insertBumpMapTexture(osg::Image *color_image, osg::Image *normal_image, osg::Image *specular_image,
        const int texture_unit_diffuse, const int texture_unit_normal, const int texture_unit_specular) {


    if (!normal_image) {
        std::cout << "NORMAL IMAGE FAIL" << std::endl;
        exit(0);
    }

    if (!color_image) {
        std::cout << "COLOR IMAGE FAIL" << std::endl;
        exit(0);
    }

    if (!specular_image) {
        std::cout << "SPECULAR IMAGE FAIL" << std::endl;
        exit(0);
    }

    osg::StateSet* bumpState = new osg::StateSet();

    // Set textures
    osg::Texture2D *normal = new osg::Texture2D();
    osg::Texture2D *color = new osg::Texture2D();
    osg::Texture2D *specular = new osg::Texture2D();

    color->setImage(color_image);
    color->setDataVariance(osg::Object::DYNAMIC);
    color->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    color->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    color->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    color->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    color->setResizeNonPowerOfTwoHint(false);
    color->setMaxAnisotropy(8.0f);

    normal->setImage(normal_image);
    normal->setDataVariance(osg::Object::DYNAMIC);
    normal->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    normal->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    normal->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    normal->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    normal->setResizeNonPowerOfTwoHint(false);
    normal->setMaxAnisotropy(8.0f);

    // specular->setImage(specular_image);
    // specular->setDataVariance(osg::Object::DYNAMIC);
    // specular->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    // specular->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    // specular->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    // specular->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    // specular->setResizeNonPowerOfTwoHint(false);
    // specular->setMaxAnisotropy(8.0f);

    bumpState->setTextureAttributeAndModes(texture_unit_diffuse, color, osg::StateAttribute::ON );
    bumpState->setTextureAttributeAndModes(texture_unit_normal, normal, osg::StateAttribute::OFF );
    // bumpState->setTextureAttributeAndModes(texture_unit_specular, specular, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);

    return bumpState;
}

osgFX::BumpMapping* bumpMapOSG(osg::Geode *geode, osg::Image *normal_image, osg::Image *difuse_image, double scale_x, double scale_y) {

    if (!normal_image || !difuse_image) {
        std::cout << "IMAGE FAIL" << std::endl;
        exit(0);
    }

    osg::StateSet* bumpState = new osg::StateSet();

    // Set textures
    osg::ref_ptr<osg::Texture2D> normal_texture(new osg::Texture2D());
    osg::ref_ptr<osg::Texture2D> difuse_texture(new osg::Texture2D());

    normal_texture->setImage(normal_image);
    normal_texture->setDataVariance(osg::Object::DYNAMIC);
    normal_texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    normal_texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    normal_texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    normal_texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    normal_texture->setResizeNonPowerOfTwoHint(false);
    normal_texture->setMaxAnisotropy(8.0f);

    difuse_texture->setImage(difuse_image);
    difuse_texture->setDataVariance(osg::Object::DYNAMIC);
    difuse_texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    difuse_texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    difuse_texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    difuse_texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    difuse_texture->setResizeNonPowerOfTwoHint(false);
    difuse_texture->setMaxAnisotropy(8.0f);

    const int TEXTURE_UNIT_NORMAL = 0;
    const int TEXTURE_UNIT_DIFUSE = 1;

    bumpState->setTextureAttributeAndModes(TEXTURE_UNIT_NORMAL, normal_texture, osg::StateAttribute::OFF );
    bumpState->setTextureAttributeAndModes(TEXTURE_UNIT_DIFUSE, difuse_texture, osg::StateAttribute::ON  );

    osg::ref_ptr<osg::Geometry> geometry = geode->asGeode()->getDrawable(0)->asGeometry();
    osg::Vec2Array* tex_coord = dynamic_cast<osg::Vec2Array*>(geometry->getTexCoordArray(0));

    for (unsigned int i = 0; i < tex_coord->getNumElements(); ++i)
        (*tex_coord)[i].set((*tex_coord)[i].x() * scale_x, (*tex_coord)[i].y() * scale_y);

    geometry->setStateSet(bumpState);
    if (tex_coord) {
        geometry->setTexCoordArray(TEXTURE_UNIT_NORMAL, tex_coord);
        geometry->setTexCoordArray(TEXTURE_UNIT_DIFUSE, tex_coord);
    } else {
        std::cout << "MISS TEXTURE COORDINATE " << std::endl;
        exit(0);
    }

    osgFX::BumpMapping* bump_mapping = new osgFX::BumpMapping();
    bump_mapping->setEnabled(true);
    bump_mapping->setLightNumber(0);
    bump_mapping->setNormalMapTextureUnit(TEXTURE_UNIT_NORMAL);
    bump_mapping->setDiffuseTextureUnit(TEXTURE_UNIT_DIFUSE);
    bump_mapping->addChild(geode);
    bump_mapping->prepareChildren();

    return bump_mapping;
}

void selectTexture(int texture, std::string *normal_path, std::string *difuse_path, std::string *specular_path) {

    std::string path = "textures/";

    std::string texture_type;
    switch (texture) {
        break;
    case 1:
        texture_type = "yellow_dirty_plastic"; //buyonacy
        break;
    case 2:
        texture_type = "white_metal_rusty"; // vortex
        break;
    case 3:
        texture_type = "white_metal_rusty2";
        break;
    case 4:
        texture_type = "yellow_texture";
        break;
    case 5:
        texture_type = "yellow_metal_scratched2";
        break;
    case 6:
        texture_type = "yellow_metal_scratched3";
        break;
    case 7:
        texture_type = "black_metal";
        break;
    case 8:
        texture_type = "dark_metal2"; //anode
        break;
    case 9:
        texture_type = "dark_metal3"; // black vortex
        break;
    case 10:
        texture_type = "anode_cover";
        break;
    case 11:
        texture_type = "anode_salt2";
        break;
    case 12:
        texture_type = "algae";
        break;
    case 13:
        texture_type = "concrete";
        break;
    case 14:
        texture_type = "subsea_moss2";
        break;
    case 15:
        texture_type = "subsea_moss3";
        break;
    case 16:
        texture_type = "subsea_moss";
        break;
    case 17:
        texture_type = "green_scratched";
        break;
    case 18:
        texture_type = "sand_texture";
        break;
    case 19:
        texture_type = "grey_metal_rusty_chipping";
        break;
    case 20:
        texture_type = "red_ral_texture";
        break;
    case 21:
        texture_type = "white_metal";
        break;
    case 22:
        texture_type = "red_scratched2";
        break;
    case 23:
        texture_type = "grey_scratched";
        break;
    case 24:
        texture_type = "grey_scratched2";
        break;
    case 25:
        texture_type = "blue_scratched";
        break;
    case 26:
        texture_type = "blue_scratched2";
        break;
    case 27:
        texture_type = "blue_scratched3";
        break;
    case 28:
        texture_type = "white_metal_chipping";
        break;
    default:
        texture_type = "grey_scratched";
        break;
    }

    (*normal_path) = path + texture_type + "_n.jpg";
    (*difuse_path) = path + texture_type + "_d.jpg";
    (*specular_path) = path + texture_type + "_s.jpg";

//    (*normal_path) = path + texture_type + "_n.png";
//    (*difuse_path) = path + texture_type + "_d.png";

}

osg::Geode* applyGeometryScaleAndTexture(osg::Group* group, osg::Geometry* geometry, int texture, double scale_x, double scale_y) {

    // scale texture process on geometry, without stateset
    geometry = scale_texture(geometry, scale_x, scale_y, TEXTURE_UNIT_SHADER_DIFFUSE, TEXTURE_UNIT_SHADER_NORMAL, TEXTURE_UNIT_SHADER_SPECULAR);
    geometry->setStateSet(new osg::StateSet);
    // insert Textures on geometry
    osg::ref_ptr<osg::Geode> geode = group->getChild(texture)->asGeode();


    geode->addDrawable(geometry);
    std::cout << " ADD NODE " << std::endl;

    // show texture applied

    std::string normal_path, difuse_path, specular_path;
    selectTexture(texture, &normal_path, &difuse_path, &specular_path);

    osg::ref_ptr<osg::Image> difuse_image = osgDB::readImageFile(difuse_path);
    osg::ref_ptr<osg::Image> normal_image = osgDB::readImageFile(normal_path);
    osg::ref_ptr<osg::Image> specular_image = osgDB::readImageFile(specular_path);

    osg::Geode* geode_new(new osg::Geode);
    geode_new->setStateSet(
            insertBumpMapTexture(difuse_image, normal_image, specular_image, TEXTURE_UNIT_SHADER_DIFFUSE, TEXTURE_UNIT_SHADER_NORMAL,
                    TEXTURE_UNIT_SHADER_SPECULAR));
    geode_new->addDrawable(geometry);
    return geode_new;

}
void checkImagesExist(std::string difuse_path, std::string normal_path, std::string specular_path){
    osg::ref_ptr<osg::Image> difuse_image = osgDB::readImageFile(difuse_path);
    osg::ref_ptr<osg::Image> normal_image = osgDB::readImageFile(normal_path);
    osg::ref_ptr<osg::Image> specular_image = osgDB::readImageFile(specular_path);
    if (!normal_image) {
        std::cout << "NORMAL IMAGE FAIL:" << normal_path << std::endl;
        exit(0);
    }

    if (!difuse_image) {
        std::cout << "COLOR IMAGE FAIL" << difuse_path <<std::endl;
        exit(0);
    }

    if (!specular_image) {
        std::cout << "SPECULAR IMAGE FAIL" << specular_path<< std::endl;
        exit(0);
    }
}

void addGeodeTexture(osg::Group* group) {

    osg::ref_ptr<osg::Geode> geode(new osg::Geode);
    group->addChild(geode);

    for (int i = 1; i <= TEXTURE_NUM; ++i) {
        std::string normal_path, difuse_path, specular_path;
        selectTexture(i, &normal_path, &difuse_path, &specular_path);

        checkImagesExist(difuse_path,normal_path, specular_path);

        osg::ref_ptr<osg::Image> difuse_image = osgDB::readImageFile(difuse_path);
        osg::ref_ptr<osg::Image> normal_image = osgDB::readImageFile(normal_path);
        osg::ref_ptr<osg::Image> specular_image = osgDB::readImageFile(specular_path);
        osg::ref_ptr<osg::Geode> geode(new osg::Geode);
        geode->setStateSet(
                insertBumpMapTexture(difuse_image, normal_image, specular_image, TEXTURE_UNIT_SHADER_DIFFUSE, TEXTURE_UNIT_SHADER_NORMAL,
                        TEXTURE_UNIT_SHADER_SPECULAR));
        group->addChild(geode);
    }
    // geode without texture


}

void removeGeodeWithoutDrawable(osg::Group* group) {

    unsigned int num_children = group->getNumChildren();
    osg::Geode* uselessChildren[num_children];

    for (unsigned int i = 0; i < num_children; ++i)
        if (group->getChild(i)->asGeode()->getNumDrawables() == 0)
            uselessChildren[i] = group->getChild(i)->asGeode();

    for (unsigned int i = 0; i < num_children; ++i) {
        group->removeChild(uselessChildren[i]);
    }

}

int main(int argc, char **argv) {
//    int bla(int argc, char **argv) {

// LOCAL TESTES SHADER BUMP MAP
//    osg::Group* bumpRoot = new osg::Group();
//// insert shader bumpMap
//    bumpRoot->setStateSet(createShaderBumpMap(TEXTURE_UNIT_SHADER_DIFFUSE, TEXTURE_UNIT_SHADER_NORMAL, TEXTURE_UNIT_SHADER_SPECULAR));
//// Insert texture in Geode
//    addGeodeTexture(bumpRoot);
//
//// Create Geometry
//    osg::ref_ptr<osg::Geometry> geometry = createSquare();
//
//// Scale texture on Geometry and make child of parent with desired texture
//    applyBumpMapShader(bumpRoot, geometry, 9, 1, 1);
//
//// remove geode without drawable geometry
//    removeGeodeWithoutDrawable(bumpRoot);
//
//    osgViewer::Viewer bumpViewer;
//    bumpViewer.setSceneData(bumpRoot);
//    bumpViewer.setCameraManipulator(new osgGA::TrackballManipulator());
//    bumpViewer.run();

//================================================================

//    // LOCAL TESTES BUMP MAP
//    osg::Group* bumpRoot = new osg::Group();
//
// // Model with bump mapping
//    osg::Geode* bumpModel = new osg::Geode;
//    bumpModel->addDrawable(createSquare());
//
//    std::string color_path, normal_path, difuse_path;
//    selectTexture(3, &normal_path, &difuse_path, &color_path);
//
//    osg::ref_ptr<osg::Image> difuse_image = osgDB::readImageFile(difuse_path);
//    osg::ref_ptr<osg::Image> normal_image = osgDB::readImageFile(normal_path);
//
//    bumpRoot->addChild(bumpMapOSG(bumpModel, normal_image, difuse_image, 1, 1));
//
//    osgViewer::Viewer bumpViewer;
//    bumpViewer.setSceneData(bumpRoot);
//    bumpViewer.setCameraManipulator(new osgGA::TrackballManipulator());
//    bumpViewer.run();

//=====================================================================================
    //
    osg::ref_ptr<osg::Group> bumpRoot = (osg::Group*) osgDB::readNodeFile("out.osg");
    if (!bumpRoot) {
        std::cout << " CREATE A NEW GROUP" << std::endl;
        bumpRoot = new osg::Group();
        // bumpRoot->setStateSet(createShaderBumpMap(TEXTURE_UNIT_SHADER_DIFFUSE, TEXTURE_UNIT_SHADER_NORMAL, TEXTURE_UNIT_SHADER_SPECULAR));
        addGeodeTexture(bumpRoot);
    }

    std::string sdf_name = "OBN";
    std::string file_name = "visual.osg";
    std::string main_path = "/home/mega/Documents/gazebo_scenes/models/sdf/";
    std::string full_path = "/home/mega/Documents/OSG_BumpMap/"; //Alternative path besides the common gazebo file structure

    std::string final_path = main_path + sdf_name + "/" + file_name;
    // std::string final_path = path + file_name; // Alternative path option

    std::cout << " path = " << final_path << std::endl;

    osg::ref_ptr<osg::Group> temp_node;

    char select = argv[1][0];

    if (select == 'r') {
      std::cout << " REMOVE USELESS GEODE" << std::endl;
      removeGeodeWithoutDrawable(bumpRoot);
      std::cout << " REMOVE DONE" << std::endl;

    } else {

      int i = atoi(argv[2]);

      osg::ref_ptr<osg::Group> original_group = (osg::Group*) osgDB::readNodeFile(final_path);//bumpMapShader/visual_wellhead_1_shader.osgb
      osg::Geode* geode_original = (osg::Geode*) original_group->getChild(i);

      double texture = atof(argv[3]);
      double scale_x = atof(argv[4]);
      double scale_y = atof(argv[5]);

      osg::Geometry* geom = scale_texture(
        geode_original->getDrawable(0)->asGeometry(),
        scale_x, scale_y,
        TEXTURE_UNIT_SHADER_DIFFUSE,
        TEXTURE_UNIT_SHADER_NORMAL,
        TEXTURE_UNIT_SHADER_SPECULAR);

      if (select == 'p'){
        std::cout << "entrou aqui" << std::endl;
        temp_node = new osg::Group();
        addGeodeTexture(temp_node);
        temp_node->getChild(texture)->asGeode()->addDrawable(geom);

      }

      if (select == 't' || select == 'f') {
        std::cout << "entrou aqui 2 " << std::endl;
        bumpRoot->getChild(texture)->asGeode()->addDrawable(geom);
      }

      int total_bumproot = 0;

      for (int i = 0; i < bumpRoot->getNumChildren(); i++)
        total_bumproot += bumpRoot->getChild(i)->asGeode()->getNumDrawables();

      int total_object = original_group->getNumChildren();
      int total_object_writen = bumpRoot->getNumChildren();

      std::cout << "OBJECT " << i;
      std::cout << " |  TOTAL ORIGINAL " << total_object;
      std::cout << " |  TOTAL OUT.OSG " << total_bumproot;
      std::cout << std::endl;

      osgViewer::Viewer bumpViewer;
      if(temp_node)
        bumpViewer.setSceneData(temp_node);
      else
        bumpViewer.setSceneData(bumpRoot);
      bumpViewer.setCameraManipulator(new osgGA::TrackballManipulator());
      bumpViewer.run();

    }

    bool write_flag = (select == 'p') || (select == 'f');
    if (  !write_flag &&
          writeOSGFileFromNode((*bumpRoot))){
        std::cout << " --> FILE WROTE!!" << std::endl;
      }

    return 0;
}
