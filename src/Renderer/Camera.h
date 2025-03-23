#pragma once

#include "niagara/math.h"
#include <SDL3/SDL.h>

/**
 * Camera class that handles camera movement and orientation
 * Manages camera position, rotation, and keyboard input handling
 */
class Camera
{
public:
    /**
     * Initializes the camera with default settings
     */
    Camera();

    /**
     * Initializes the camera with specific settings
     * 
     * @param position Initial camera position
     * @param orientation Initial camera orientation as quaternion
     * @param fovY Vertical field of view in radians
     * @param znear Near clip plane distance
     */
    Camera(const vec3& position, const quat& orientation, float fovY, float znear);
    
    /**
     * Updates camera position and orientation based on keyboard input
     * 
     * @param deltaTime Time since last frame in seconds
     */
    void update(float deltaTime);
    
    /**
     * Calculates and returns the camera's view matrix
     * 
     * @return The view matrix for the current camera state
     */
    mat4 getViewMatrix() const;
    
    /**
     * Calculates and returns the camera's projection matrix
     * 
     * @param aspectRatio The current viewport aspect ratio
     * @param zfar Far clip plane distance
     * @return The projection matrix for the current camera state
     */
    mat4 getProjectionMatrix(float aspectRatio, float zfar) const;

    /**
     * Get camera position
     * 
     * @return Current camera position
     */
    vec3 getPosition() const { return m_position; }
    
    /**
     * Set camera position
     * 
     * @param position New camera position
     */
    void setPosition(const vec3& position) { m_position = position; }
    
    /**
     * Get camera orientation
     * 
     * @return Current camera orientation as quaternion
     */
    quat getOrientation() const { return m_orientation; }
    
    /**
     * Set camera orientation
     * 
     * @param orientation New camera orientation as quaternion
     */
    void setOrientation(const quat& orientation) { m_orientation = orientation; }
    
    /**
     * Get camera field of view
     * 
     * @return Vertical field of view in radians
     */
    float getFovY() const { return m_fovY; }
    
    /**
     * Set camera field of view
     * 
     * @param fovY New vertical field of view in radians
     */
    void setFovY(float fovY) { m_fovY = fovY; }
    
    /**
     * Get camera near clip plane
     * 
     * @return Near clip plane distance
     */
    float getZNear() const { return m_znear; }
    
    /**
     * Set camera near clip plane
     * 
     * @param znear New near clip plane distance
     */
    void setZNear(float znear) { m_znear = znear; }

private:
    vec3 m_position;         // Camera position in world space
    quat m_orientation;      // Camera orientation as quaternion
    float m_fovY;            // Vertical field of view in radians
    float m_znear;           // Near clip plane distance
    
    float m_moveSpeed;       // Movement speed units per second
    float m_rotateSpeed;     // Rotation speed radians per second
    
    /**
     * Processes keyboard input and updates camera position/orientation
     * 
     * @param deltaTime Time since last frame in seconds
     */
    void processKeyboardInput(float deltaTime);
};