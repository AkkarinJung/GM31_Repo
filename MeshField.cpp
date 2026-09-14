#include "main.h"
#include "renderer.h"
#include "MeshField.h"
#include "audio.h"

float g_FieldHeight[21][21] =
{
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,5.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
    {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},

};

// The grass material is a whole set of maps, so keep the loading in one place.
static void LoadTexture(const wchar_t* FileName, ID3D11ShaderResourceView** Texture)
{
    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(FileName, WIC_FLAGS_NONE, &metadata, image);

    // WIC hands back the full size image and nothing else. Tiling the ground
    // this many times means a distant pixel covers a wide patch of texture,
    // and with no smaller mip to read it turns into crawling noise instead of
    // grass. Build the chain so the sampler has something to fall back to.
    ScratchImage mipChain;
    HRESULT hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), metadata,
        TEX_FILTER_DEFAULT, 0, mipChain);

    if (SUCCEEDED(hr))
    {
        CreateShaderResourceView(Renderer::GetDevice(), mipChain.GetImages(),
            mipChain.GetImageCount(), mipChain.GetMetadata(), Texture);
    }
    else
    {
        CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
            image.GetImageCount(), metadata, Texture);
    }
    assert(*Texture);
}

// Texture tiles per grid cell. Cells are 10 units across and the player is
// 1.8 units tall, so at 1 tile per cell a single painted flower came out as
// wide as the player. 3 puts a tile every 3.3 units, which reads as grass;
// much past 4 and the repeat starts showing as a grid.
static const float TEXTURE_TILING = 3.0f;

void MeshField::Init()
{
    m_Layer = 1;

    // 頂点バッファ生成
    {
        for (int x = 0; x < 21; x++)
        {
            for (int z = 0; z < 21; z++)
            {
                m_Vertex[x][z].Position = XMFLOAT3((x - 10) * 10.0f, g_FieldHeight[z][x], (z - 10) * -10.0f);
                m_Vertex[x][z].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
                m_Vertex[x][z].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                m_Vertex[x][z].TexCoord = XMFLOAT2(x * TEXTURE_TILING, z * TEXTURE_TILING);
            }
        }

        //法線計算
        for (int x = 1; x < 20; x++)
        {
            for (int z = 1; z < 20; z++)
            {
                Vector3 vx, vz, vn;
                vx.x = m_Vertex[x + 1][z].Position.x - m_Vertex[x - 1][z].Position.x;
                vx.y = m_Vertex[x + 1][z].Position.y - m_Vertex[x - 1][z].Position.y;
                vx.z = m_Vertex[x + 1][z].Position.z - m_Vertex[x - 1][z].Position.z;

                vz.x = m_Vertex[x][z - 1].Position.x - m_Vertex[x][z + 1].Position.x;
                vz.y = m_Vertex[x][z - 1].Position.y - m_Vertex[x][z + 1].Position.y;
                vz.z = m_Vertex[x][z - 1].Position.z - m_Vertex[x][z + 1].Position.z;

                vn = Vector3::cross(vz, vx);//外積
                vn.normalize();//正規化（長さ1にする）

                m_Vertex[x][z].Normal.x = vn.x;
                m_Vertex[x][z].Normal.y = vn.y;
                m_Vertex[x][z].Normal.z = vn.z;
            }
        }

        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(VERTEX_3D) * 21 * 21;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = m_Vertex;

        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);
    }

    //インデックスバッファ生成
    {
        unsigned int index[(22 * 2) * 20 - 2];

        int i = 0;
        for (int x = 0; x < 20; x++)
        {
            for (int z = 0; z < 21; z++)
            {
                index[i] = x * 21 + z;
                i++;

                index[i] = (x + 1) * 21 + z;
                i++;
            }

            if (x == 19)
                break;

            //縮退ポリゴン
            index[i] = (x + 1) * 21 + 20;
            i++;

            index[i] = (x + 1) * 21;
            i++;
        }

        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(unsigned int) * ((22 * 2) * 20 - 2);
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = index;
        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_IndexBuffer);
    }

    
 
    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\BumpVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\BumpPS.cso");

    LoadTexture(L"asset\\texture\\grass\\animegrass_albedo.png", &m_Texture);
    LoadTexture(L"asset\\texture\\grass\\animegrass_normal.png", &m_TextureNormal);
    LoadTexture(L"asset\\texture\\grass\\animegrass_height.png", &m_TextureHeight);
    LoadTexture(L"asset\\texture\\grass\\animegrass_roughness.png", &m_TextureRoughness);
    LoadTexture(L"asset\\texture\\grass\\animegrass_ao.png", &m_TextureAO);
    LoadTexture(L"asset\\texture\\grass\\animegrass_metallic.png", &m_TextureMetallic);

    // The same ramp the enemies shade through, so the ground bands the same way.
    LoadTexture(L"asset\\texture\\toon_ramp.png", &m_TextureRamp);

    //BGM
    Audio* bgm = AddGameComponent<Audio>(this);
    bgm->Load("asset\\Audio\\BGM\\GameBGM.wav");
    bgm->Play(true);

}
void MeshField::Uninit()
{
    m_VertexBuffer->Release();
    m_IndexBuffer->Release();


    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    m_Texture->Release();
    m_TextureNormal->Release();
    m_TextureHeight->Release();
    m_TextureRoughness->Release();
    m_TextureAO->Release();
    m_TextureMetallic->Release();
    m_TextureRamp->Release();

    GameObject::Uninit();
}
void  MeshField::Update()
{
    GameObject::Update();

}
void  MeshField::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    MATERIAL material{};
    material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
    material.Ambient = { 1.0f, 1.0f, 1.0f, 1.0f };
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    // Knobs for BumpPS: x = ramp row, y = AO strength, z = macro variation,
    // w = normal map strength. Enough of the normal map to catch the light
    // without fighting the shading already painted into the texture.
    Renderer::SetParameter(XMFLOAT4(0.12f, 1.0f, 1.0f, 0.35f));

    ID3D11ShaderResourceView* textures[] =
    {
        m_Texture,          // t0 albedo
        m_TextureNormal,    // t1 normal
        m_TextureHeight,    // t2 height
        m_TextureRoughness, // t3 roughness
        m_TextureAO,        // t4 ambient occlusion
        m_TextureMetallic,  // t5 metallic
        m_TextureRamp,      // t6 toon ramp
    };
    Renderer::GetDeviceContext()->PSSetShaderResources(0, ARRAYSIZE(textures), textures);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer, DXGI_FORMAT_R32_UINT, 0);


    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    GameObject::Draw();

    //Renderer::GetDeviceContext()->Draw(21 * 21, 0);
    Renderer::GetDeviceContext()->DrawIndexed((22 * 2) * 20 - 2, 0, 0);
}

float MeshField::GetHeight(Vector3 Position)
{
    int x, z;

    x = (int)(Position.x / 10.0f + 10.0f);
    z = (int)(Position.z / -10.0f + 10.0f);

    XMFLOAT3 pos0, pos1, pos2, pos3;
    pos0 = m_Vertex[x][z].Position;
    pos1 = m_Vertex[x + 1][z].Position;
    pos2 = m_Vertex[x][z + 1].Position;
    pos3 = m_Vertex[x + 1][z + 1].Position;

    Vector3 v12, v1p;
    v12.x = pos2.x - pos1.x;
    v12.y = pos2.y - pos1.y;
    v12.z = pos2.z - pos1.z;

    v1p.x = Position.x - pos1.x;
    v1p.y = Position.y - pos1.y;
    v1p.z = Position.z - pos1.z;

    float cy = v12.z * v1p.x - v12.x * v1p.z;

    float py;
    Vector3 n;

    if (cy > 0.0f)
    {
        Vector3 v10;
        v10.x = pos0.x - pos1.x;
        v10.y = pos0.y - pos1.y;
        v10.z = pos0.z - pos1.z;

        n = Vector3::cross(v10, v12);
    }
    else
    {
        Vector3 v13;
        v13.x = pos3.x - pos1.x;
        v13.y = pos3.y - pos1.y;
        v13.z = pos3.z - pos1.z;

        n = Vector3::cross(v12, v13);
    }


    py = -((Position.x - pos1.x) * n.x
        + (Position.z - pos1.z) * n.z) / n.y + pos1.y;

    return py;
}
