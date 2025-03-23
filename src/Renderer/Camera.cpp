#include "Camera.h"
#include "RendererUtils.h"

Camera::Camera()
    : m_position(0.0f, 0.0f, 0.0f)
    , m_orientation(0.0f, 0.0f, 0.0f, 1.0f)
    , m_fovY(glm::radians(70.0f))
    , m_znear(0.1f)
    , m_moveSpeed(5.0f)
    , m_rotateSpeed(2.0f)
{
}

Camera::Camera(const vec3& position, const quat& orientation, float fovY, float znear)
    : m_position(position)
    , m_orientation(orientation)
    , m_fovY(fovY)
    , m_znear(znear)
    , m_moveSpeed(5.0f)
    , m_rotateSpeed(2.0f)
{
}

void Camera::update(float deltaTime)
{
    processKeyboardInput(deltaTime);
}

mat4 Camera::getViewMatrix() const
{
    // Calculate view matrix manually
    mat4 view = glm::mat4_cast(m_orientation);
    view[3] = vec4(m_position, 1.0f);
    view = inverse(view);
    // Flip Z for Vulkan coordinate system
    return glm::scale(glm::identity<glm::mat4>(), vec3(1, 1, -1)) * view;
}

mat4 Camera::getProjectionMatrix(float aspectRatio, float zfar) const
{
    // Create a perspective projection matrix with reverse-Z for better precision in the distance
    float tanHalfFovy = tan(m_fovY / 2.0f);
    
    mat4 result(0.0f);
    result[0][0] = 1.0f / (aspectRatio * tanHalfFovy);
    result[1][1] = 1.0f / tanHalfFovy;
    result[2][2] = 0.0f;
    result[2][3] = -1.0f;
    result[3][2] = m_znear;
    result[3][3] = 0.0f;
    
    return result;
}

void Camera::processKeyboardInput(float deltaTime)
{
    // Simulate dummy key states for now
    // In the real implementation, this would use SDL3's keyboard state API
    // For now, we're just demonstrating the structure of the camera movement code
    bool keyW = true;   // Always move forward for demo
    bool keyS = false;
    bool keyA = false;
    bool keyD = false;
    bool keyQ = false;
    bool keyE = false;
    bool keyUp = false;
    bool keyDown = false;
    bool keyLeft = false;
    bool keyRight = false;
    
    // Calculate forward and right vectors from orientation
    vec3 forward = vec3(0, 0, -1);
    vec3 right = vec3(1, 0, 0);
    vec3 up = vec3(0, 1, 0);
    
    // Transform vectors by camera orientation
    forward = m_orientation * forward;
    right = m_orientation * right;
    up = m_orientation * up;
    
    // Calculate movement speed with delta time
    float moveDelta = m_moveSpeed * deltaTime;
    
    // Forward/backward movement (W/S)
    if (keyW)
        m_position += forward * moveDelta;
    if (keyS)
        m_position -= forward * moveDelta;
    
    // Left/right movement (A/D)
    if (keyA)
        m_position -= right * moveDelta;
    if (keyD)
        m_position += right * moveDelta;
    
    // Up/down movement (Q/E)
    if (keyQ)
        m_position += up * moveDelta;
    if (keyE)
        m_position -= up * moveDelta;
    
    // Calculate rotation speed with delta time
    float rotateDelta = m_rotateSpeed * deltaTime;
    
    // Rotation around axes (arrow keys)
    vec3 eulerAngles(0.0f);
    
    if (keyUp)
        eulerAngles.x -= rotateDelta;
    if (keyDown)
        eulerAngles.x += rotateDelta;
    
    if (keyLeft)
        eulerAngles.y -= rotateDelta;
    if (keyRight)
        eulerAngles.y += rotateDelta;
    
    // Apply rotation if any key was pressed
    if (eulerAngles.x != 0.0f || eulerAngles.y != 0.0f || eulerAngles.z != 0.0f)
    {
        // Create quaternion from euler angles
        quat rotation = quat(eulerAngles);
        
        // Apply rotation to camera orientation
        m_orientation = normalize(rotation * m_orientation);
    }
}