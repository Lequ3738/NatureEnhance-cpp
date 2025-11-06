import os
import subprocess
import shutil

def main():
    source_exe = r"C:\Project\GMK\NatureEnhance-cpp\Release\NatureEnhance-cpp.dll"
    new_name = "Main.dll"
    
    directory = os.path.dirname(source_exe)
    
    if not os.path.isfile(source_exe):
        print(f"错误: 文件不存在 - {source_exe}")
        input("按回车键退出...")
        return
    
    try:
        print(f"开始压缩: {os.path.basename(source_exe)}...")
        
        cmd = f'upx --best "{source_exe}"'
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        
        new_file = os.path.join(directory, new_name)
        
        shutil.move(source_exe, new_file)
        
        print(f"\n操作成功完成!")
        
    except Exception as e:
        print(f"处理过程中出错: {str(e)}")
    
    input("\n按回车键退出...")

if __name__ == "__main__":
    main()