#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>

void Check(bool value,const char* message) {
    if (!value) { std::cerr<<"FAIL: "<<message<<'\n'; std::exit(1); }
}
int main() {
    std::size_t files=0,triangles=0;
    for (const auto& entry:std::filesystem::directory_iterator("project/Resources/farm/contest/ceremony")) {
        if (entry.path().extension()!=".obj") continue;
        Assimp::Importer importer;
        const auto* scene=importer.ReadFile(entry.path().string(),
            aiProcess_Triangulate|aiProcess_FlipWindingOrder|aiProcess_FlipUVs);
        if (!scene) std::cerr<<entry.path()<<": "<<importer.GetErrorString()<<'\n';
        Check(scene && scene->mRootNode && scene->HasMeshes(),"valid import with root/meshes");
        for (unsigned m=0;m<scene->mNumMeshes;++m) {
            const auto* mesh=scene->mMeshes[m];
            Check(mesh && mesh->HasNormals() && mesh->HasTextureCoords(0),"shader attribute contract");
            Check(mesh->mMaterialIndex<scene->mNumMaterials,"material index");
            Check(!mesh->HasBones(),"static mesh only");
            for (unsigned i=0;i<mesh->mNumVertices;++i) {
                const auto& p=mesh->mVertices[i]; const auto& n=mesh->mNormals[i]; const auto& uv=mesh->mTextureCoords[0][i];
                Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"finite vertex");
                Check(std::abs(n.SquareLength()-1)<.001f,"unit normal");
                Check(uv.x>=0 && uv.x<=1 && uv.y>=0 && uv.y<=1,"bounded UV");
            }
            for (unsigned i=0;i<mesh->mNumFaces;++i) {
                const auto& face=mesh->mFaces[i];
                Check(face.mNumIndices==3,"triangulation");
                for (unsigned j=0;j<3;++j) Check(face.mIndices[j]<mesh->mNumVertices,"bounded index");
                ++triangles;
            }
        }
        ++files;
    }
    Check(files==17 && triangles<65000,"fixed file and triangle budget");
    std::cout<<"PASS "<<files<<" imported meshes; "<<triangles<<" triangles; normals/UV/indices\n";
}
